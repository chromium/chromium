// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MIN_TEXTBOX_SIZE_PX} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {Ink2Manager, InkTextBoxElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertPositionAndSize, dragHandleWithKeyboard, getTestAnnotation, initializeBox, setupTextBoxTest, verifyFinishTextAnnotationMessage} from './ink2_text_box_test_utils.js';
import {getRequiredElement} from './test_util.js';

async function setUpExistingAnnotation(
    manager: Ink2Manager, textbox: InkTextBoxElement) {
  // Initialize and commit a new annotation to make it "existing".
  initializeBox(manager, 100, 100, 55, 10);
  await microtasksFinished();
  const testAnnotation =
      getTestAnnotation({locationX: 0, locationY: 7, height: 100, width: 100});
  textbox.$.textbox.value = testAnnotation.text;
  textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
  await microtasksFinished();
  keyDownOn(textbox, 0, [], 'Escape');
  await microtasksFinished();
  chrome.test.assertTrue(textbox.hidden);

  // Re-initialize the box as an existing annotation by simulating a click on
  // it.
  const clicked = await manager.initializeTextAnnotation({x: 105, y: 60});
  chrome.test.assertTrue(clicked, 'Failed to click existing annotation');
  await microtasksFinished();
  chrome.test.assertFalse(textbox.hidden);
  chrome.test.assertTrue(isVisible(textbox));
  chrome.test.assertEq('Hello World', textbox.$.textbox.value);

  return testAnnotation;
}

chrome.test.runTests([
  async function testResizeWithKeyboard() {
    const {manager, textbox} = await setupTextBoxTest();
    // Initialize to a 100x200 box at 400, 300.
    initializeBox(manager, 100, 200, 400, 300);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    assertPositionAndSize(textbox, '124px', '220px', '383px', '285px');

    // 5px up and left.
    const topLeft = getRequiredElement(textbox, '.handle.top.left');
    await dragHandleWithKeyboard(topLeft, 'ArrowUp', 5);
    await dragHandleWithKeyboard(topLeft, 'ArrowLeft', 5);
    assertPositionAndSize(textbox, '129px', '225px', '378px', '280px');

    // Top handle 3px up and left. Left arrow is ignored on this handle.
    const top = getRequiredElement(textbox, '.handle.top.center');
    await dragHandleWithKeyboard(top, 'ArrowUp', 3);
    await dragHandleWithKeyboard(top, 'ArrowLeft', 3);
    assertPositionAndSize(textbox, '129px', '228px', '378px', '277px');

    // Top right handle 4px down and right -> makes box shorter and wider.
    // Use focusout event to finish right arrow drag to ensure it works.
    const topRight = getRequiredElement(textbox, '.handle.top.right');
    await dragHandleWithKeyboard(topRight, 'ArrowDown', 4);
    await dragHandleWithKeyboard(topRight, 'ArrowRight', 4, true);
    assertPositionAndSize(textbox, '133px', '224px', '378px', '281px');

    // Drag the left handle right and up. Upward motion is ignored. Left motion
    // makes the box 2px narrower.
    const left = getRequiredElement(textbox, '.handle.left.center');
    await dragHandleWithKeyboard(left, 'ArrowUp', 2);
    await dragHandleWithKeyboard(left, 'ArrowRight', 2);
    assertPositionAndSize(textbox, '131px', '224px', '380px', '281px');

    // Drag the right handle right and down. Downward motion is ignored. Right
    // motion makes the box 3px wider.
    const right = getRequiredElement(textbox, '.handle.right.center');
    await dragHandleWithKeyboard(right, 'ArrowDown', 3);
    await dragHandleWithKeyboard(right, 'ArrowRight', 3);
    assertPositionAndSize(textbox, '134px', '224px', '380px', '281px');

    // Drag the bottom left handle down and left to make the box 10px bigger
    // in both dimensions.
    const bottomLeft = getRequiredElement(textbox, '.handle.bottom.left');
    await dragHandleWithKeyboard(bottomLeft, 'ArrowDown', 10);
    await dragHandleWithKeyboard(bottomLeft, 'ArrowLeft', 10);
    assertPositionAndSize(textbox, '144px', '234px', '370px', '281px');

    // Drag the bottom handle down and left to make the box 5px taller.
    // Motion left is ignored.
    const bottom = getRequiredElement(textbox, '.handle.bottom.center');
    await dragHandleWithKeyboard(bottom, 'ArrowDown', 5);
    await dragHandleWithKeyboard(bottom, 'ArrowLeft', 5);
    assertPositionAndSize(textbox, '144px', '239px', '370px', '281px');

    // Drag the bottom right handle down and right to make the box 2px bigger
    // in both dimensions.
    const bottomRight = getRequiredElement(textbox, '.handle.bottom.right');
    await dragHandleWithKeyboard(bottomRight, 'ArrowDown', 2);
    await dragHandleWithKeyboard(bottomRight, 'ArrowRight', 2);
    assertPositionAndSize(textbox, '146px', '241px', '370px', '281px');

    // Drag the bottom right handle up and left to try to make the box too
    // small. Make sure it clamps at the same minimum size, anchored on the top
    // left corner.
    await dragHandleWithKeyboard(bottomRight, 'ArrowUp', 100);
    await dragHandleWithKeyboard(bottomRight, 'ArrowLeft', 100);
    const clampedWidth = textbox.$.textbox.clientWidth;
    const clampedHeight = textbox.$.textbox.clientHeight;
    chrome.test.assertTrue(clampedHeight >= textbox.$.textbox.scrollHeight);
    chrome.test.assertEq(MIN_TEXTBOX_SIZE_PX + 10, clampedWidth);
    assertPositionAndSize(
        textbox, '48px', `${clampedHeight + 14}px`, '370px', '281px');

    chrome.test.succeed();
  },

  async function testMoveWithKeyboard() {
    const {manager, textbox} = await setupTextBoxTest();
    // Initialize to a 100x100 box at 400, 300.
    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '120px', '383px', '285px');
    await dragHandleWithKeyboard(textbox, 'ArrowUp', 5);
    assertPositionAndSize(textbox, '124px', '120px', '383px', '280px');
    await dragHandleWithKeyboard(textbox, 'ArrowDown', 5);
    assertPositionAndSize(textbox, '124px', '120px', '383px', '285px');
    await dragHandleWithKeyboard(textbox, 'ArrowRight', 5);
    assertPositionAndSize(textbox, '124px', '120px', '388px', '285px');
    await dragHandleWithKeyboard(textbox, 'ArrowLeft', 5);
    assertPositionAndSize(textbox, '124px', '120px', '383px', '285px');

    // Make sure that arrow keys in the textarea are ignored.
    await dragHandleWithKeyboard(textbox.$.textbox, 'ArrowUp', 1);
    await dragHandleWithKeyboard(textbox.$.textbox, 'ArrowDown', 1);
    await dragHandleWithKeyboard(textbox.$.textbox, 'ArrowLeft', 1);
    await dragHandleWithKeyboard(textbox.$.textbox, 'ArrowRight', 1);
    assertPositionAndSize(textbox, '124px', '120px', '383px', '285px');
    chrome.test.succeed();
  },

  async function testEscape() {
    const {manager, mockPlugin, textbox, viewport} = await setupTextBoxTest();
    viewport.setZoom(1.0);

    // Initialize to a 100x100 box at 55, 10. Place the box in the top corner
    // of the page, so that the viewport won't scroll when it is focused.
    initializeBox(manager, 100, 100, 55, 10);
    // Wait for focus to happen so that we can correctly test focus changes
    // later.
    await eventToPromise('textbox-focused-for-test', textbox);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));

    // Editing text --> commit annotation on event.
    const testAnnotation = getTestAnnotation(
        {locationX: 0, locationY: 7, height: 100, width: 100});

    mockPlugin.clearMessages();
    textbox.$.textbox.value = testAnnotation.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    // Escape on the textarea blurs the textarea to focus the top level box.
    // Wait for the test-only focus event, as browser focus/blur events may
    // be flaky in tests.
    const whenFocused =
        eventToPromise('ink-text-box-focused-for-test', textbox);
    keyDownOn(textbox.$.textbox, 0, [], 'Escape');
    await whenFocused;
    // Textbox is still visible, because this event does not commit the
    // annotation.
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));

    // Escape on the textbox commits the annotation and hides the box.
    keyDownOn(textbox, 0, [], 'Escape');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));
    verifyFinishTextAnnotationMessage(mockPlugin, testAnnotation, true);

    chrome.test.succeed();
  },

  async function testEscapeWhileDragging() {
    const {manager, mockPlugin, textbox, viewport} = await setupTextBoxTest();
    viewport.setZoom(1.0);

    // If the user is dragging, escape commits the annotation at the start
    // location and hides the box.
    initializeBox(manager, 100, 100, 55, 10);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    mockPlugin.clearMessages();
    const testAnnotation = getTestAnnotation(
        {locationX: 0, locationY: 7, height: 100, width: 100});
    textbox.$.textbox.value = testAnnotation.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    const handle = getRequiredElement(textbox, '.handle.bottom.right');
    handle.dispatchEvent(new PointerEvent(
        'pointerdown', {composed: true, pointerId: 1, clientX: 0, clientY: 0}));
    handle.dispatchEvent(new PointerEvent(
        'pointerdown',
        {composed: true, pointerId: 1, clientX: 10, clientY: 10}));
    await microtasksFinished();
    keyDownOn(textbox, 0, [], 'Escape');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));
    // Message is identical to before because 'pointerup' was never fired.
    verifyFinishTextAnnotationMessage(mockPlugin, testAnnotation, true);

    chrome.test.succeed();
  },

  async function testEscapeWithoutModifications() {
    const {manager, mockPlugin, textbox} = await setupTextBoxTest();
    // Escape without any modification hides the box but doesn't send a message.
    // This should also work when the Escape key is on some other element in the
    // document, and not on the textbox itself.
    initializeBox(manager, 100, 100, 55, 10);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    mockPlugin.clearMessages();
    keyDownOn(document.body, 0, [], 'Escape');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('setTextAnnotation'));

    chrome.test.succeed();
  },

  async function testDeleteWithBackspaceKey() {
    const {manager, mockPlugin, textbox} = await setupTextBoxTest();
    const testAnnotation = await setUpExistingAnnotation(manager, textbox);

    mockPlugin.clearMessages();
    keyDownOn(textbox, 0, [], 'Backspace');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));

    testAnnotation.text = '';
    verifyFinishTextAnnotationMessage(mockPlugin, testAnnotation, true);

    chrome.test.succeed();
  },

  async function testDeleteWithDeleteKey() {
    const {manager, mockPlugin, textbox} = await setupTextBoxTest();
    const testAnnotation = await setUpExistingAnnotation(manager, textbox);

    mockPlugin.clearMessages();
    keyDownOn(textbox, 0, [], 'Delete');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));

    testAnnotation.text = '';
    verifyFinishTextAnnotationMessage(mockPlugin, testAnnotation, true);

    chrome.test.succeed();
  },
]);
