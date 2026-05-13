// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UndoRedoStack} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {UndoRedoStateChangedDetail} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';

import {assertDeepEquals} from './test_util.js';

let lastEventDetail: UndoRedoStateChangedDetail|null = null;

function createStackForEventTest(): UndoRedoStack {
  lastEventDetail = null;
  const target = new EventTarget();
  const stack = new UndoRedoStack(target);
  target.addEventListener('undo-redo-state-changed', (e: Event) => {
    lastEventDetail = (e as CustomEvent<UndoRedoStateChangedDetail>).detail;
  });
  return stack;
}

function assertEvent(canUndo: boolean, canRedo: boolean, isDirty: boolean) {
  assert(lastEventDetail);
  chrome.test.assertEq(canUndo, lastEventDetail.canUndo);
  chrome.test.assertEq(canRedo, lastEventDetail.canRedo);
  chrome.test.assertEq(isDirty, lastEventDetail.hasUnsavedEdits);
  lastEventDetail = null;
}

function assertNoEvent() {
  chrome.test.assertEq(null, lastEventDetail);
}

chrome.test.runTests([
  function testPushAndUndoRedo() {
    const stack = createStackForEventTest();

    // Initial state
    chrome.test.assertFalse(stack.canUndo());
    chrome.test.assertFalse(stack.canRedo());
    chrome.test.assertFalse(stack.isDirty());

    // Undo when there is nothing to undo is a no-op and doesn't fire an event.
    chrome.test.assertEq(null, stack.undo());
    assertNoEvent();

    // Push ink annotation
    stack.push({type: 'ink'});
    assertEvent(true, false, true);

    // Push text annotation
    stack.push({type: 'text'});
    assertEvent(true, false, true);

    // Undo (pops the text annotation)
    assertDeepEquals({type: 'text'}, stack.undo());
    assertEvent(true, true, true);

    // Undo again pops the ink annotation, and resets to original clean state.
    assertDeepEquals({type: 'ink'}, stack.undo());
    assertEvent(false, true, false);

    // Undo when there is nothing to undo is a no-op and doesn't fire an event.
    chrome.test.assertEq(null, stack.undo());
    assertNoEvent();

    // Redo pushes the ink annotation
    assertDeepEquals({type: 'ink'}, stack.redo());
    assertEvent(true, true, true);

    // Redo again pushes the text annotation
    assertDeepEquals({type: 'text'}, stack.redo());
    assertEvent(true, false, true);

    // Calling redo with nothing to redo does not fire an event and is a no-op.
    chrome.test.assertEq(null, stack.redo());
    assertNoEvent();

    chrome.test.succeed();
  },

  function testSavedState() {
    const stack = createStackForEventTest();

    // Initial state is clean.
    chrome.test.assertFalse(stack.isDirty());

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
    stack.push({type: 'text'});
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

    chrome.test.succeed();
  },

  function testInvalidateRedo() {
    const stack = createStackForEventTest();

    stack.push({type: 'ink'});
    assertEvent(true, false, true);
    stack.push({type: 'text'});
    assertEvent(true, false, true);

    // Pointer is now pointing to the ink change, and text can be redone.
    stack.undo();
    assertEvent(true, true, true);

    // Pushing a new annotation should prevent redo, because the state should
    // be cleared.
    stack.push({type: 'ink'});
    assertEvent(true, false, true);

    chrome.test.succeed();
  },
]);
