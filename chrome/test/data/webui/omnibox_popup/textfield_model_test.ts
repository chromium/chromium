// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TextfieldModel} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('TextfieldModelTest', function() {
  let model: TextfieldModel;

  setup(() => {
    model = new TextfieldModel();
  });

  test('InitialState', () => {
    assertFalse(model.canUndo());
    assertFalse(model.canRedo());
    assertEquals(0, model.length);
    assertEquals(-1, model.currentEditIndex);
  });

  test('SingleInsertUndoRedo', () => {
    model.insertText('hello');
    assertTrue(model.canUndo());
    assertFalse(model.canRedo());
    assertEquals(1, model.length);

    assertTrue(!!model.undo());
    assertEquals('', model.text);
    assertEquals(0, model.selection.start);
    assertEquals(0, model.selection.end);
    assertFalse(model.canUndo());
    assertTrue(model.canRedo());

    assertTrue(!!model.redo());
    assertEquals('hello', model.text);
    assertEquals(5, model.selection.start);
    assertEquals(5, model.selection.end);
    assertTrue(model.canUndo());
    assertFalse(model.canRedo());
  });

  test('MergeableSequentialInserts', () => {
    model.insertChar('a');
    model.insertChar('b');
    model.insertChar('c');

    assertEquals(1, model.length);

    assertTrue(!!model.undo());
    assertEquals('', model.text);
    assertEquals(0, model.selection.start);
  });

  test('NonMergeableInserts', () => {
    model.insertChar('a');
    model.paste('x');

    assertEquals(2, model.length);

    assertTrue(!!model.undo());
    assertEquals('a', model.text);

    assertTrue(!!model.undo());
    assertEquals('', model.text);
  });

  test('CommitPreventsMerge', () => {
    model.insertChar('a');
    model.commitCurrentEdit();
    model.insertChar('b');

    assertEquals(2, model.length);
  });

  test('NewEditTruncatesRedoStack', () => {
    model.insertText('first');
    model.commitCurrentEdit();
    model.insertText('second');

    assertTrue(!!model.undo());
    assertTrue(model.canRedo());

    model.insertText('third');

    assertFalse(model.canRedo());
    assertEquals(2, model.length);
  });

  test('SingleDeleteUndoRedo', () => {
    model.setText('abc', 3);
    model.clearEditHistory();
    assertTrue(model.backspace());

    assertTrue(model.canUndo());
    assertEquals(1, model.length);

    assertTrue(!!model.undo());
    assertEquals('abc', model.text);
    assertEquals(3, model.selection.start);
    assertEquals(3, model.selection.end);

    assertTrue(!!model.redo());
    assertEquals('ab', model.text);
    assertEquals(2, model.selection.start);
    assertEquals(2, model.selection.end);
  });

  test('MergeableBackspaceDeletions', () => {
    model.setText('abc', 3);
    model.clearEditHistory();
    assertTrue(model.backspace());
    assertTrue(model.backspace());
    assertTrue(model.backspace());

    assertEquals(1, model.length);

    assertTrue(!!model.undo());
    assertEquals('abc', model.text);
    assertEquals(3, model.selection.start);
  });

  test('MergeableForwardDeletions', () => {
    model.setText('abc', 0);
    model.clearEditHistory();
    assertTrue(model.delete());
    assertTrue(model.delete());
    assertTrue(model.delete());

    assertEquals(1, model.length);

    assertTrue(!!model.undo());
    assertEquals('abc', model.text);
    assertEquals(0, model.selection.start);
  });

  test('SelectionDeleteUndoRedo', () => {
    model.setText('abc', 0);
    model.selectRange({start: 1, end: 3});
    model.clearEditHistory();
    assertTrue(model.backspace());

    assertTrue(!!model.undo());
    assertEquals('abc', model.text);
    assertEquals(1, model.selection.start);
    assertEquals(3, model.selection.end);
  });

  test('SingleReplaceUndoRedo', () => {
    model.setText('hello', 0);
    model.selectRange({start: 1, end: 4});
    model.clearEditHistory();
    model.insertChar('a');

    assertTrue(model.canUndo());
    assertEquals(1, model.length);

    assertTrue(!!model.undo());
    assertEquals('hello', model.text);
    assertEquals(1, model.selection.start);
    assertEquals(4, model.selection.end);

    assertTrue(!!model.redo());
    assertEquals('hao', model.text);
    assertEquals(2, model.selection.start);
    assertEquals(2, model.selection.end);
  });

  test('PasteReplaceSelectionUndoRedo', () => {
    model.setText('hello', 0);
    model.selectRange({start: 1, end: 4});
    model.clearEditHistory();
    model.paste('world');

    assertTrue(!!model.undo());
    assertEquals('hello', model.text);
    assertEquals(1, model.selection.start);
    assertEquals(4, model.selection.end);

    assertTrue(!!model.redo());
    assertEquals('hworldo', model.text);
    assertEquals(6, model.selection.start);
    assertEquals(6, model.selection.end);
  });

  test('ReplaceEditMergeWithInserts', () => {
    model.setText('world', 0);
    model.selectRange({start: 0, end: 5});
    model.clearEditHistory();
    model.insertChar('u');
    model.insertChar('n');
    model.insertChar('i');

    assertEquals(1, model.length);

    assertTrue(!!model.undo());
    assertEquals('world', model.text);
    assertEquals(0, model.selection.start);
    assertEquals(5, model.selection.end);
  });

  test('UndoRedo_BasicTest', () => {
    model.insertChar('a');
    assertFalse(!!model.redo());  // There is nothing to redo.
    assertTrue(!!model.undo());
    assertEquals('', model.text);
    assertTrue(!!model.redo());
    assertEquals('a', model.text);

    // Continuous inserts are treated as one edit.
    model.insertChar('b');
    model.insertChar('c');
    assertEquals('abc', model.text);
    assertTrue(!!model.undo());
    assertEquals('a', model.text);
    assertEquals(1, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('', model.text);
    assertEquals(0, model.getCursorPosition());

    // Undoing further shouldn't change the text.
    assertFalse(!!model.undo());
    assertEquals('', model.text);
    assertFalse(!!model.undo());
    assertEquals('', model.text);
    assertEquals(0, model.getCursorPosition());

    // Redoing to the latest text.
    assertTrue(!!model.redo());
    assertEquals('a', model.text);
    assertEquals(1, model.getCursorPosition());
    assertTrue(!!model.redo());
    assertEquals('abc', model.text);
    assertEquals(3, model.getCursorPosition());

    // Backspace ===============================
    assertTrue(model.backspace());
    assertEquals('ab', model.text);
    assertTrue(!!model.undo());
    assertEquals('abc', model.text);
    assertEquals(3, model.getCursorPosition());
    assertTrue(!!model.redo());
    assertEquals('ab', model.text);
    assertEquals(2, model.getCursorPosition());
    // Continuous backspaces are treated as one edit.
    assertTrue(model.backspace());
    assertTrue(model.backspace());
    assertEquals('', model.text);
    // Extra backspace shouldn't affect the history.
    assertFalse(model.backspace());
    assertTrue(!!model.undo());
    assertEquals('ab', model.text);
    assertEquals(2, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('abc', model.text);
    assertEquals(3, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('a', model.text);
    assertEquals(1, model.getCursorPosition());

    // Clear history
    model.clearEditHistory();
    assertFalse(!!model.undo());
    assertFalse(!!model.redo());
    assertEquals('a', model.text);
    assertEquals(1, model.getCursorPosition());

    // Delete ===============================
    model.setText('ABCDE', 0);
    model.clearEditHistory();
    model.selectRange({start: 2, end: 2});
    assertTrue(model.delete());
    assertEquals('ABDE', model.text);
    model.selectRange({start: 0, end: 0});
    assertTrue(model.delete());
    assertEquals('BDE', model.text);
    assertTrue(!!model.undo());
    assertEquals('ABDE', model.text);
    assertEquals(0, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('ABCDE', model.text);
    assertEquals(2, model.getCursorPosition());
    assertTrue(!!model.redo());
    assertEquals('ABDE', model.text);
    assertEquals(2, model.getCursorPosition());
    // Continuous deletes are treated as one edit.
    assertTrue(model.delete());
    assertTrue(model.delete());
    assertEquals('AB', model.text);
    assertTrue(!!model.undo());
    assertEquals('ABDE', model.text);
    assertEquals(2, model.getCursorPosition());
    assertTrue(!!model.redo());
    assertEquals('AB', model.text);
    assertEquals(2, model.getCursorPosition());
  });

  test('UndoRedo_SetText', () => {
    // Simulate typing www.y while www.google.com and www.youtube.com are
    // autocompleted.
    model.insertChar('w');  //                     w|
    assertEquals('w', model.text);
    assertEquals(1, model.getCursorPosition());
    model.setText('www.google.com', 1);  //   w|ww.google.com
    model.selectRange(
        {start: 14, end: 1});  //                  w[ww.google.com]
    assertEquals(1, model.getCursorPosition());
    assertEquals('www.google.com', model.text);
    model.insertChar('w');  //                     ww|
    assertEquals('ww', model.text);
    model.setText('www.google.com', 2);  //   ww|w.google.com
    model.selectRange(
        {start: 14, end: 2});  //                  ww[w.google.com]
    model.insertChar('w');     //                  www|
    assertEquals('www', model.text);
    model.setText('www.google.com', 3);  //   www|.google.com
    model.selectRange(
        {start: 14, end: 3});  //                  www[.google.com]
    model.insertChar('.');     //                  www.|
    assertEquals('www.', model.text);
    model.setText('www.google.com', 4);  //   www.|google.com
    model.selectRange(
        {start: 14, end: 4});  //                  www.[google.com]
    model.insertChar('y');     //                  www.y|
    assertEquals('www.y', model.text);
    model.setText('www.youtube.com', 5);  //  www.y|outube.com
    assertEquals('www.youtube.com', model.text);
    assertEquals(5, model.getCursorPosition());

    // Undo until the initial edit.
    assertTrue(!!model.undo());
    assertEquals('www.google.com', model.text);
    assertEquals(4, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('www.google.com', model.text);
    assertEquals(3, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('www.google.com', model.text);
    assertEquals(2, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('www.google.com', model.text);
    assertEquals(1, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('', model.text);
    assertEquals(0, model.getCursorPosition());

    assertFalse(!!model.undo());

    // Redo until the last edit.
    assertTrue(!!model.redo());
    assertEquals('www.google.com', model.text);
    assertEquals(1, model.getCursorPosition());
    assertTrue(!!model.redo());
    assertEquals('www.google.com', model.text);
    assertEquals(2, model.getCursorPosition());
    assertTrue(!!model.redo());
    assertEquals('www.google.com', model.text);
    assertEquals(3, model.getCursorPosition());
    assertTrue(!!model.redo());
    assertEquals('www.google.com', model.text);
    assertEquals(4, model.getCursorPosition());
    assertTrue(!!model.redo());
    assertEquals('www.youtube.com', model.text);
    assertEquals(5, model.getCursorPosition());
    assertFalse(!!model.redo());
  });

  test('UndoRedo_BackspaceThenSetText', () => {
    model.insertChar('w');
    assertEquals('w', model.text);
    assertEquals(1, model.getCursorPosition());
    model.setText('www.google.com', 1);
    assertEquals('www.google.com', model.text);
    assertEquals(1, model.getCursorPosition());
    model.selectRange({start: 14, end: 14});
    assertEquals(14, model.getCursorPosition());
    assertTrue(model.backspace());
    assertTrue(model.backspace());
    assertEquals('www.google.c', model.text);
    // Autocomplete sets the text.
    model.setText('www.google.com/search=www.google.c', 12);
    assertEquals('www.google.com/search=www.google.c', model.text);
    assertEquals(12, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('www.google.c', model.text);
    assertEquals(12, model.getCursorPosition());
    assertTrue(!!model.undo());
    assertEquals('www.google.com', model.text);
    assertEquals(14, model.getCursorPosition());
  });

  test('UndoRedo_CutCopyPasteTest', () => {
    model.setText('ABCDE', 5);
    assertFalse(!!model.redo());  // There is nothing to redo.
    // Test Cut.
    model.selectRange({start: 1, end: 3});  //                       A[BC]DE
    assertEquals(3, model.getCursorPosition());
    model.cut();  //                                                 A|DE
    assertEquals('ADE', model.text);
    assertEquals(1, model.getCursorPosition());
    assertTrue(!!model.undo());  //                                  A[BC]DE
    assertEquals('ABCDE', model.text);
    assertEquals(3, model.getCursorPosition());
    assertEquals(1, model.selection.start);
    assertEquals(3, model.selection.end);
    assertTrue(!!model.undo());  //                                  |
    assertEquals('', model.text);
    assertEquals(0, model.getCursorPosition());
    assertFalse(!!model.undo());  // There is no more to undo.       |
    assertEquals('', model.text);
    assertTrue(!!model.redo());  //                                  ABCDE|
    assertEquals('ABCDE', model.text);
    assertEquals(5, model.getCursorPosition());
    assertTrue(!!model.redo());  //                                  A|DE
    assertEquals('ADE', model.text);
    assertEquals(1, model.getCursorPosition());
    assertFalse(!!model.redo());  // There is no more to redo.       A|DE
    assertEquals('ADE', model.text);

    model.paste('BC');  //                                           ABC|DE
    model.paste('BC');  //                                           ABCBC|DE
    model.paste('BC');  //                                           ABCBCBC|DE
    assertEquals('ABCBCBCDE', model.text);
    assertEquals(7, model.getCursorPosition());
    assertTrue(!!model.undo());  //                                  ABCBC|DE
    assertEquals('ABCBCDE', model.text);
    assertEquals(5, model.getCursorPosition());
    assertTrue(!!model.undo());  //                                  ABC|DE
    assertEquals('ABCDE', model.text);
    assertEquals(3, model.getCursorPosition());
    assertTrue(!!model.undo());  //                                  A|DE
    assertEquals('ADE', model.text);
    assertEquals(1, model.getCursorPosition());
    assertTrue(!!model.undo());  //                                  A[BC]DE
    assertEquals('ABCDE', model.text);
    assertEquals(3, model.getCursorPosition());
    assertEquals(1, model.selection.start);
    assertEquals(3, model.selection.end);
    assertTrue(!!model.undo());  //                                  |
    assertEquals('', model.text);
    assertEquals(0, model.getCursorPosition());
    assertFalse(!!model.undo());  //                                 |
    assertEquals('', model.text);
    assertTrue(!!model.redo());
    assertEquals('ABCDE', model.text);  //                           ABCDE|
    assertEquals(5, model.getCursorPosition());

    // Test Redo.
    assertTrue(!!model.redo());  //                                  A|DE
    assertEquals('ADE', model.text);
    assertEquals(1, model.getCursorPosition());
    assertTrue(!!model.redo());  //                                  ABC|DE
    assertEquals('ABCDE', model.text);
    assertEquals(3, model.getCursorPosition());
    assertTrue(!!model.redo());  //                                  ABCBC|DE
    assertEquals('ABCBCDE', model.text);
    assertEquals(5, model.getCursorPosition());
    assertTrue(!!model.redo());  //                                  ABCBCBC|DE
    assertEquals('ABCBCBCDE', model.text);
    assertEquals(7, model.getCursorPosition());
    assertFalse(!!model.redo());  //                                 ABCBCBC|DE

    // Test using SelectRange.
    model.selectRange(
        {start: 1, end: 3});  //                                     A[BC]BCBCDE
    assertTrue(model.cut());  //                                     A|BCBCDE
    assertEquals('ABCBCDE', model.text);
    assertEquals(1, model.getCursorPosition());
    model.selectRange({start: 1, end: 1});  //                       A|BCBCDE
    assertFalse(model.cut());  //                                    A|BCBCDE
    model.selectRange({start: 7, end: 7});  //                       ABCBCDE|
    assertTrue(model.paste('BC'));  //                               ABCBCDEBC|
    assertEquals('ABCBCDEBC', model.text);
    assertEquals(9, model.getCursorPosition());
    assertTrue(!!model.undo());  //                                  ABCBCDE|
    assertEquals('ABCBCDE', model.text);
    assertEquals(7, model.getCursorPosition());
    // An empty cut shouldn't create an edit.
    assertTrue(!!model.undo());  //                                  ABC|BCBCDE
    assertEquals('ABCBCBCDE', model.text);
    assertEquals(3, model.getCursorPosition());
    assertEquals(1, model.selection.start);
    assertEquals(3, model.selection.end);
  });

  test('Undo_SelectionTest', () => {
    const range = {start: 2, end: 4};
    model.setText('abcdef', 0);
    model.clearEditHistory();
    model.selectRange(range);
    assertEquals(range.start, model.selection.start);
    assertEquals(range.end, model.selection.end);

    // Deleting the selected text should change the text and the range.
    assertTrue(model.backspace());
    assertEquals('abef', model.text);
    assertEquals(2, model.selection.start);
    assertEquals(2, model.selection.end);

    // Undoing the deletion should restore the former range.
    assertTrue(!!model.undo());
    assertEquals('abcdef', model.text);
    assertEquals(range.start, model.selection.start);
    assertEquals(range.end, model.selection.end);

    // When range.start = range.end, nothing is selected and
    // range.start = range.end = cursor position
    model.selectRange({start: 2, end: 2});
    assertEquals(2, model.selection.start);
    assertEquals(2, model.selection.end);

    // Deleting a single character should change the text and cursor location.
    assertTrue(model.backspace());
    assertEquals('acdef', model.text);
    assertEquals(1, model.selection.start);
    assertEquals(1, model.selection.end);

    // Undoing the deletion should restore the former range.
    assertTrue(!!model.undo());
    assertEquals('abcdef', model.text);
    assertEquals(2, model.selection.start);
    assertEquals(2, model.selection.end);

    model.selectRange({start: model.text.length, end: model.text.length});
    assertTrue(model.backspace());
    model.selectRange({start: 1, end: 3});
    model.setText('[set]', 0);
    assertTrue(!!model.undo());
    assertEquals('abcde', model.text);
    assertEquals(1, model.selection.start);
    assertEquals(3, model.selection.end);
  });

  function runInsertReplaceTest(testModel: TextfieldModel) {
    const reverse = testModel.selection.start > testModel.selection.end;
    testModel.insertChar('1');
    testModel.insertChar('2');
    testModel.insertChar('3');
    assertEquals('a123d', testModel.text);
    assertEquals(4, testModel.getCursorPosition());
    assertTrue(!!testModel.undo());
    assertEquals('abcd', testModel.text);
    assertEquals(reverse ? 1 : 3, testModel.getCursorPosition());
    assertTrue(!!testModel.undo());
    assertEquals('', testModel.text);
    assertEquals(0, testModel.getCursorPosition());
    assertFalse(!!testModel.undo());
    assertTrue(!!testModel.redo());
    assertEquals('abcd', testModel.text);
    assertEquals(4, testModel.getCursorPosition());
    assertTrue(!!testModel.redo());
    assertEquals('a123d', testModel.text);
    assertEquals(4, testModel.getCursorPosition());
    assertFalse(!!testModel.redo());
  }

  test('UndoRedo_ReplaceTest', () => {
    // Select forwards and insert.
    {
      const testModel = new TextfieldModel();
      testModel.setText('abcd', 4);
      testModel.selectRange({start: 1, end: 3});
      runInsertReplaceTest(testModel);
    }
    // Select reversed and insert.
    {const testModel = new TextfieldModel(); testModel.setText('abcd', 4);
     testModel.selectRange({start: 3, end: 1});
     runInsertReplaceTest(testModel);}
  });
});
