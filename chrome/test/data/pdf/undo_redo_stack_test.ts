// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UndoRedoStack} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {UndoRedoState, UndoRedoStateChangedDetail} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNull} from 'chrome://webui-test/chai_assert.js';

const textState: UndoRedoState = {
  type: 'text',
  before: null,
  after: null,
};

suite('UndoRedoStack', () => {
  let stack: UndoRedoStack;
  let lastEventDetail: UndoRedoStateChangedDetail|null = null;

  setup(() => {
    lastEventDetail = null;
    const target = new EventTarget();
    stack = new UndoRedoStack(target);
    target.addEventListener('undo-redo-state-changed', (e: Event) => {
      lastEventDetail = (e as CustomEvent<UndoRedoStateChangedDetail>).detail;
    });
  });

  function assertEvent(canUndo: boolean, canRedo: boolean, isDirty: boolean) {
    assert(lastEventDetail);
    assertEquals(canUndo, lastEventDetail.canUndo);
    assertEquals(canRedo, lastEventDetail.canRedo);
    assertEquals(isDirty, lastEventDetail.hasUnsavedEdits);
    lastEventDetail = null;
  }

  function assertNoEvent() {
    assertNull(lastEventDetail);
  }

  test('PushAndUndoRedo', () => {
    // Initial state
    assertFalse(stack.canUndo());
    assertFalse(stack.canRedo());
    assertFalse(stack.isDirty());

    // Undo when there is nothing to undo is a no-op and doesn't fire an event.
    assertNull(stack.undo());
    assertNoEvent();

    // Push ink annotation
    stack.push({type: 'ink'});
    assertEvent(true, false, true);

    // Push text annotation
    stack.push(textState);
    assertEvent(true, false, true);

    // Undo (pops the text annotation)
    assertDeepEquals(textState, stack.undo());
    assertEvent(true, true, true);

    // Undo again pops the ink annotation, and resets to original clean state.
    assertDeepEquals({type: 'ink'}, stack.undo());
    assertEvent(false, true, false);

    // Undo when there is nothing to undo is a no-op and doesn't fire an event.
    assertNull(stack.undo());
    assertNoEvent();

    // Redo pushes the ink annotation
    assertDeepEquals({type: 'ink'}, stack.redo());
    assertEvent(true, true, true);

    // Redo again pushes the text annotation
    assertDeepEquals(textState, stack.redo());
    assertEvent(true, false, true);

    // Calling redo with nothing to redo does not fire an event and is a no-op.
    assertNull(stack.redo());
    assertNoEvent();
  });

  test('SavedState', () => {
    // Initial state is clean.
    assertFalse(stack.isDirty());

    // Ink stroke makes the stack dirty.
    stack.push({type: 'ink'});
    assertEvent(true, false, true);

    // Saving clears the dirty bit.
    stack.initiateSave();
    assertNoEvent();
    stack.setSaved();
    assertEvent(true, false, false);

    // Saving again without new changes does not fire an event.
    stack.initiateSave();
    assertNoEvent();
    stack.setSaved();
    assertNoEvent();

    // New text annotation -> dirty
    stack.push(textState);
    assertEvent(true, false, true);

    // Undo to saved state -> clean
    stack.undo();
    assertEvent(true, true, false);

    // Undo past saved state -> dirty
    stack.undo();
    assertEvent(false, true, true);

    // Redo to saved state -> clean
    stack.redo();
    assertEvent(true, true, false);
  });

  test('InvalidateRedo', () => {
    stack.push({type: 'ink'});
    assertEvent(true, false, true);
    stack.push(textState);
    assertEvent(true, false, true);

    // Pointer is now pointing to the ink change, and text can be redone.
    stack.undo();
    assertEvent(true, true, true);

    // Pushing a new annotation should prevent redo, because the state should
    // be cleared.
    stack.push({type: 'ink'});
    assertEvent(true, false, true);
  });
});
