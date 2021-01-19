open EditorCoreTypes;
open Vim;
open TestFramework;

let resetBuffer = () =>
  Helpers.resetBuffer("test/reason-libvim/testfile.txt");

let input =
    (
      ~autoClosingPairs=AutoClosingPairs.empty,
      ~mode=Mode.Normal({cursor: BytePosition.zero}),
      key,
    ) => {
  let (out, _effects: list(Effect.t)) =
    Vim.input(~context={...Context.current(), autoClosingPairs, mode}, key);

  Context.(out.mode);
};

let key =
    (
      ~autoClosingPairs=AutoClosingPairs.empty,
      ~mode=Mode.Insert({cursors: []}),
      key,
    ) => {
  let (out, _effects: list(Effect.t)) =
    Vim.key(
      ~context=Context.{...Context.current(), autoClosingPairs, mode},
      key,
    );

  Context.(out.mode);
};

describe("Multi-cursor", ({describe, _}) => {
  describe("visual block mode", ({test, _}) => {
    let hasCursorMatching = (~lineIndex, ~byteIndex, cursors) => {
      EditorCoreTypes.(
        cursors
        |> List.exists((cursor: BytePosition.t) =>
             LineNumber.toZeroBased(cursor.line) == lineIndex
             && ByteIndex.toInt(cursor.byte) == byteIndex
           )
      );
    };
    test("expand to multiple cursors with 'I'", ({expect, _}) => {
      let _: Buffer.t = resetBuffer();
      let (context, _) = Vim.key("<c-v>");
      let (context, _) = Vim.input(~context, "j");
      let (context, _) = Vim.input(~context, "j");

      expect.equal(Vim.Mode.isVisual(context.mode), true);

      let (context, _) = Vim.input(~context, "I");

      expect.equal(Vim.Mode.isInsert(context.mode), true);

      let cursors = Vim.Mode.cursors(context.mode);
      expect.equal(List.length(cursors), 3);

      expect.equal(
        cursors |> hasCursorMatching(~lineIndex=0, ~byteIndex=0),
        true,
      );
      expect.equal(
        cursors |> hasCursorMatching(~lineIndex=1, ~byteIndex=0),
        true,
      );
      expect.equal(
        cursors |> hasCursorMatching(~lineIndex=2, ~byteIndex=0),
        true,
      );
    });
    test("expand to multiple cursors with 'A'", ({expect, _}) => {
      let _: Buffer.t = resetBuffer();
      let (context, _) = Vim.key("<c-v>");
      let (context, _) = Vim.input(~context, "j");
      let (context, _) = Vim.input(~context, "j");

      expect.equal(Vim.Mode.isVisual(context.mode), true);

      let (context, _) = Vim.input(~context, "A");

      expect.equal(Vim.Mode.isInsert(context.mode), true);

      let cursors = Vim.Mode.cursors(context.mode);
      expect.equal(List.length(cursors), 3);

      expect.equal(
        cursors |> hasCursorMatching(~lineIndex=0, ~byteIndex=1),
        true,
      );
      expect.equal(
        cursors |> hasCursorMatching(~lineIndex=1, ~byteIndex=1),
        true,
      );
      expect.equal(
        cursors |> hasCursorMatching(~lineIndex=2, ~byteIndex=1),
        true,
      );
    });
  });
  describe("normal mode", ({describe, _}) => {
    describe("single cursor", ({test, _}) => {
      test("set cursor works as expected", ({expect, _}) => {
        let _: Buffer.t = resetBuffer();
        let mode1 = input("j");

        mode1
        |> Vim.Mode.cursors
        |> List.hd
        |> (
          cursor => expect.int(cursor.line |> LineNumber.toZeroBased).toBe(1)
        );

        expect.int(
          Cursor.get() |> BytePosition.line |> LineNumber.toZeroBased,
        ).
          toBe(
          1,
        );

        // set cursor, and move up
        let mode2 =
          input(
            ~mode=
              Normal({
                cursor:
                  BytePosition.{
                    line: LineNumber.ofZeroBased(2),
                    byte: ByteIndex.zero,
                  },
              }),
            "k",
          );

        mode2
        |> Vim.Mode.cursors
        |> List.hd
        |> (
          cursor =>
            expect.int(cursor |> BytePosition.line |> LineNumber.toZeroBased).
              toBe(
              1,
            )
        );

        expect.int(
          Cursor.get() |> BytePosition.line |> LineNumber.toZeroBased,
        ).
          toBe(
          1,
        );
      })
    })
  });
  describe("insert mode", ({describe, test, _}) => {
    describe("undo", ({test, _}) => {
      test("undo multiple lines", ({expect, _}) => {
        let buf = resetBuffer();
        let mode =
          input(
            ~mode=
              Vim.Mode.Insert({
                cursors: [
                  BytePosition.{line: LineNumber.zero, byte: ByteIndex.zero},
                  BytePosition.{
                    line: LineNumber.(zero + 1),
                    byte: ByteIndex.zero,
                  },
                  BytePosition.{
                    line: LineNumber.(zero + 2),
                    byte: ByteIndex.zero,
                  },
                ],
              }),
            "a",
          );

        let line1 = Buffer.getLine(buf, LineNumber.zero);
        let line2 = Buffer.getLine(buf, LineNumber.(zero + 1));
        let line3 = Buffer.getLine(buf, LineNumber.(zero + 2));

        expect.string(line1).toEqual(
          "aThis is the first line of a test file",
        );
        expect.string(line2).toEqual(
          "aThis is the second line of a test file",
        );
        expect.string(line3).toEqual(
          "aThis is the third line of a test file",
        );

        let mode' = key(~mode, "<esc>");
        let _mode'' = key(~mode=mode', "u");

        let line1 = Buffer.getLine(buf, LineNumber.zero);
        let line2 = Buffer.getLine(buf, LineNumber.(zero + 1));
        let line3 = Buffer.getLine(buf, LineNumber.(zero + 2));

        expect.string(line1).toEqual(
          "This is the first line of a test file",
        );
        expect.string(line2).toEqual(
          "This is the second line of a test file",
        );
        expect.string(line3).toEqual(
          "This is the third line of a test file",
        );
      })
    });
    test("multi-cursor auto-closing pairs", ({expect, _}) => {
      let buf = resetBuffer();

      let autoClosingPairs =
        AutoClosingPairs.create(
          AutoClosingPairs.[AutoPair.{opening: "{", closing: "}"}],
        );

      let updates: ref(list(BufferUpdate.t)) = ref([]);
      let dispose = Buffer.onUpdate(upd => updates := [upd, ...updates^]);

      let (_context: Context.t, _effects: list(Effect.t)) = Vim.input("i");
      expect.int(List.length(updates^)).toBe(0);

      let mode =
        input(
          ~autoClosingPairs,
          ~mode=
            Vim.Mode.Insert({
              cursors: [
                BytePosition.{line: LineNumber.zero, byte: ByteIndex.zero},
                BytePosition.{
                  line: LineNumber.(zero + 1),
                  byte: ByteIndex.zero,
                },
                BytePosition.{
                  line: LineNumber.(zero + 2),
                  byte: ByteIndex.zero,
                },
              ],
            }),
          "{",
        );

      let line1 = Buffer.getLine(buf, LineNumber.zero);
      let line2 = Buffer.getLine(buf, LineNumber.(zero + 1));
      let line3 = Buffer.getLine(buf, LineNumber.(zero + 2));

      expect.string(line1).toEqual(
        "{}This is the first line of a test file",
      );
      expect.string(line2).toEqual(
        "{}This is the second line of a test file",
      );
      expect.string(line3).toEqual(
        "{}This is the third line of a test file",
      );

      let _: Vim.Mode.t = input(~autoClosingPairs, ~mode, "a");

      let line1 = Buffer.getLine(buf, LineNumber.zero);
      let line2 = Buffer.getLine(buf, LineNumber.(zero + 1));
      let line3 = Buffer.getLine(buf, LineNumber.(zero + 2));

      expect.string(line1).toEqual(
        "{a}This is the first line of a test file",
      );
      expect.string(line2).toEqual(
        "{a}This is the second line of a test file",
      );
      expect.string(line3).toEqual(
        "{a}This is the third line of a test file",
      );

      dispose();
    });
    test("insert / backspace", ({expect, _}) => {
      let buf = resetBuffer();

      let updates: ref(list(BufferUpdate.t)) = ref([]);
      let dispose = Buffer.onUpdate(upd => updates := [upd, ...updates^]);

      let (_: Context.t, _: list(Effect.t)) = Vim.input("i");
      expect.int(List.length(updates^)).toBe(0);

      let mode =
        input(
          ~mode=
            Insert({
              cursors: [
                BytePosition.{line: LineNumber.zero, byte: ByteIndex.zero},
                BytePosition.{
                  line: LineNumber.(zero + 1),
                  byte: ByteIndex.zero,
                },
                BytePosition.{
                  line: LineNumber.(zero + 2),
                  byte: ByteIndex.zero,
                },
              ],
            }),
          "a",
        );

      let line1 = Buffer.getLine(buf, LineNumber.zero);
      let line2 = Buffer.getLine(buf, LineNumber.(zero + 1));
      let line3 = Buffer.getLine(buf, LineNumber.(zero + 2));

      expect.string(line1).toEqual("aThis is the first line of a test file");
      expect.string(line2).toEqual(
        "aThis is the second line of a test file",
      );
      expect.string(line3).toEqual("aThis is the third line of a test file");

      let mode = input(~mode, "b");

      let line1 = Buffer.getLine(buf, LineNumber.zero);
      let line2 = Buffer.getLine(buf, LineNumber.(zero + 1));
      let line3 = Buffer.getLine(buf, LineNumber.(zero + 2));

      expect.string(line1).toEqual(
        "abThis is the first line of a test file",
      );
      expect.string(line2).toEqual(
        "abThis is the second line of a test file",
      );
      expect.string(line3).toEqual(
        "abThis is the third line of a test file",
      );

      let _: Vim.Mode.t = key(~mode, "<bs>");

      let line1 = Buffer.getLine(buf, LineNumber.zero);
      let line2 = Buffer.getLine(buf, LineNumber.(zero + 1));
      let line3 = Buffer.getLine(buf, LineNumber.(zero + 2));

      expect.string(line1).toEqual("aThis is the first line of a test file");
      expect.string(line2).toEqual(
        "aThis is the second line of a test file",
      );
      expect.string(line3).toEqual("aThis is the third line of a test file");

      dispose();
    });
  });
});
