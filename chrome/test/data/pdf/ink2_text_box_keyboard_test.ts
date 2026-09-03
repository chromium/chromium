// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {TextStyle} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkTextBoxElement, TextAnnotation, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {keyDownOn, keyUpOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertPositionAndSize, dragHandleWithKeyboard, getCtrlModifier, getStrikethroughKey, getStrikethroughModifiers, getTestAnnotation, initializeBox, reactivateBox, setupTextBoxTest, verifyFinishTextAnnotationMessage} from './ink2_text_box_test_utils.js';
import {getRequiredElement} from './test_util.js';

async function setUpExistingAnnotation(
    textbox: InkTextBoxElement, viewport: Viewport) {
  // Initialize and commit a new annotation to make it "existing".
  initializeBox(100, 100, 55, 10);
  await microtasksFinished();
  const testAnnotation =
      getTestAnnotation({locationX: 0, locationY: 7, height: 100, width: 100});
  textbox.$.textbox.value = testAnnotation.text;
  textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
  await microtasksFinished();
  keyDownOn(textbox, 0, [], 'Escape');
  await microtasksFinished();
  chrome.test.assertTrue(textbox.hidden);

  // Re-initialize the box as an existing annotation by setting properties
  // directly.
  reactivateBox(textbox, viewport, testAnnotation);
  await microtasksFinished();
  chrome.test.assertFalse(textbox.hidden);
  chrome.test.assertTrue(isVisible(textbox));
  chrome.test.assertEq('Hello World', textbox.$.textbox.value);

  return testAnnotation;
}

chrome.test.runTests([
  async function testResizeWithKeyboard() {
    const {textbox} = await setupTextBoxTest();
    // Initialize to a 100x200 box at 400, 300.
    initializeBox(100, 200, 400, 300);
    await microtasksFinished();
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertTrue(isVisible(textbox));
    assertPositionAndSize(textbox, '124px', '220px', '383px', '285px');

    const modifiers: Parameters<typeof keyDownOn>[2] =
        isMac ? ['meta', 'ctrl'] : ['ctrl', 'alt'];

    // Resize larger horizontally: Ctrl + Alt + b (+10px)
    keyDownOn(textbox, 0, modifiers, 'b');
    await microtasksFinished();
    assertPositionAndSize(textbox, '134px', '220px', '383px', '285px');

    // Resize smaller horizontally: Ctrl + Alt + w (-10px)
    keyDownOn(textbox, 0, modifiers, 'w');
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '220px', '383px', '285px');

    // Resize larger vertically: Ctrl + Alt + i (+10px)
    keyDownOn(textbox, 0, modifiers, 'i');
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '230px', '383px', '285px');

    // Resize smaller vertically: Ctrl + Alt + 9 (-10px)
    keyDownOn(textbox, 0, modifiers, '9');
    await microtasksFinished();
    assertPositionAndSize(textbox, '124px', '220px', '383px', '285px');

    // Proportional resize larger: Ctrl + Alt + k (x1.1)
    // 100x200 -> 110x220 (styled 134x240)
    keyDownOn(textbox, 0, modifiers, 'k');
    await microtasksFinished();
    assertPositionAndSize(textbox, '134px', '240px', '383px', '285px');

    // Proportional resize smaller: Ctrl + Alt + j (x0.9)
    // 110x220 -> 99x198 (styled 123x218)
    keyDownOn(textbox, 0, modifiers, 'j');
    await microtasksFinished();
    assertPositionAndSize(textbox, '123px', '218px', '383px', '285px');

    // Shrink to minimum size and ensure it clamps.
    for (let i = 0; i < 20; i++) {
      keyDownOn(textbox, 0, modifiers, 'j');
      await microtasksFinished();
    }
    // Clamps when width hits MIN_TEXTBOX_SIZE_PX (24) -> styled 48x66 (drifted
    // from 1:2 due to rounding)
    assertPositionAndSize(textbox, '48px', '66px', '383px', '285px');

    // Expand horizontally until it hits the right page boundary (max width 610
    // -> styled 634).
    for (let i = 0; i < 70; i++) {
      keyDownOn(textbox, 0, modifiers, 'b');
      await microtasksFinished();
    }
    assertPositionAndSize(textbox, '634px', '66px', '383px', '285px');

    // Expand vertically until it hits the bottom page boundary (max height 703
    // -> styled 723).
    for (let i = 0; i < 80; i++) {
      keyDownOn(textbox, 0, modifiers, 'i');
      await microtasksFinished();
    }
    assertPositionAndSize(textbox, '634px', '723px', '383px', '285px');

    // Try to scale larger when already at max size. It should not grow.
    keyDownOn(textbox, 0, modifiers, 'k');
    await microtasksFinished();
    assertPositionAndSize(textbox, '634px', '723px', '383px', '285px');

    // Reset the box to 300x400 at 400, 300 to test proportional resize
    // clamping.
    initializeBox(300, 400, 400, 300);
    await microtasksFinished();
    assertPositionAndSize(textbox, '324px', '420px', '383px', '285px');

    // Scale larger repeatedly. Height should hit the limit (703) first,
    // and width should clamp at 527 (styled 551px) to preserve 3:4 aspect
    // ratio.
    for (let i = 0; i < 10; i++) {
      keyDownOn(textbox, 0, modifiers, 'k');
      await microtasksFinished();
    }
    assertPositionAndSize(textbox, '551px', '723px', '383px', '285px');

    chrome.test.succeed();
  },

  async function testMoveWithKeyboard() {
    const {textbox} = await setupTextBoxTest();
    // Initialize to a 100x100 box at 400, 300.
    initializeBox(100, 100, 400, 300);
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
    const {mockPlugin, textbox, viewport} = await setupTextBoxTest();
    viewport.setZoom(1.0);

    // Initialize to a 100x100 box at 55, 10. Place the box in the top corner
    // of the page, so that the viewport won't scroll when it is focused.
    initializeBox(100, 100, 55, 10);
    // Wait for focus to ensure focus changes can be correctly tested later.
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
    const {mockPlugin, textbox, viewport} = await setupTextBoxTest();
    viewport.setZoom(1.0);

    // If the user is dragging, escape commits the annotation at the start
    // location and hides the box.
    initializeBox(100, 100, 55, 10);
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
    const {mockPlugin, textbox} = await setupTextBoxTest();
    // Escape without any modification hides the box but doesn't send a message.
    // This should also work when the Escape key is on some other element in the
    // document, and not on the textbox itself.
    initializeBox(100, 100, 55, 10);
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
    const {mockPlugin, textbox, viewport} = await setupTextBoxTest();
    const testAnnotation = await setUpExistingAnnotation(textbox, viewport);

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
    const {mockPlugin, textbox, viewport} = await setupTextBoxTest();
    const testAnnotation = await setUpExistingAnnotation(textbox, viewport);

    mockPlugin.clearMessages();
    keyDownOn(textbox, 0, [], 'Delete');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));

    testAnnotation.text = '';
    verifyFinishTextAnnotationMessage(mockPlugin, testAnnotation, true);

    chrome.test.succeed();
  },

  async function testArrowKeysPropagation() {
    const {textbox} = await setupTextBoxTest();

    // Initialize to a 100x100 box at 400, 300.
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();

    let keydownEvents: KeyboardEvent[] = [];
    const listener = (e: KeyboardEvent) => {
      keydownEvents.push(e);
    };
    window.addEventListener('keydown', listener);

    // Case 1: Focused on the outer textbox (moving).
    // Arrow keys should not propagate.
    for (const key of ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight']) {
      keydownEvents = [];
      keyDownOn(textbox, 0, [], key);
      await microtasksFinished();
      chrome.test.assertEq(
          0, keydownEvents.length,
          `Key ${key} on textbox should not propagate`);
    }

    // Case 2: After Esc (committed and inactive).
    // Arrow keys should propagate.
    keyDownOn(textbox.$.textbox, 0, [], 'Escape');
    await microtasksFinished();
    // Escape on the outer box commits it.
    keyDownOn(textbox, 0, [], 'Escape');
    await microtasksFinished();
    chrome.test.assertTrue(textbox.hidden);
    chrome.test.assertFalse(isVisible(textbox));

    // Now send arrows to document.body. They should propagate to window.
    for (const key of ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight']) {
      keydownEvents = [];
      keyDownOn(document.body, 0, [], key);
      await microtasksFinished();
      chrome.test.assertEq(
          1, keydownEvents.length, `Key ${key} after commit should propagate`);
      chrome.test.assertEq(key, keydownEvents[0]!.key);
    }

    window.removeEventListener('keydown', listener);
    chrome.test.succeed();
  },

  async function testA11yAnnouncements() {
    const {textbox} = await setupTextBoxTest();
    initializeBox(100, 200, 400, 300);
    await microtasksFinished();

    const modifiers: Parameters<typeof keyDownOn>[2] =
        isMac ? ['meta', 'ctrl'] : ['ctrl', 'alt'];

    // Resize via keyboard shortcut.
    let whenAnnounced = eventToPromise<CustomEvent<{messages: string[]}>>(
        'cr-a11y-announcer-messages-sent', document.body);
    keyDownOn(textbox, 0, modifiers, 'b');
    await microtasksFinished();
    keyUpOn(textbox, 0, modifiers, 'b');
    let event = await whenAnnounced;
    chrome.test.assertTrue(event.detail.messages.includes(
        loadTimeData.getString('ink2TextAnnotationResized')));

    // Move up via arrow keys.
    whenAnnounced = eventToPromise<CustomEvent<{messages: string[]}>>(
        'cr-a11y-announcer-messages-sent', document.body);
    await dragHandleWithKeyboard(textbox, 'ArrowUp', 5);
    event = await whenAnnounced;
    chrome.test.assertTrue(event.detail.messages.includes(
        loadTimeData.getString('ink2TextAnnotationMovedUp')));

    // Move down via arrow keys.
    whenAnnounced = eventToPromise<CustomEvent<{messages: string[]}>>(
        'cr-a11y-announcer-messages-sent', document.body);
    await dragHandleWithKeyboard(textbox, 'ArrowDown', 5);
    event = await whenAnnounced;
    chrome.test.assertTrue(event.detail.messages.includes(
        loadTimeData.getString('ink2TextAnnotationMovedDown')));

    // Move left via arrow keys.
    whenAnnounced = eventToPromise<CustomEvent<{messages: string[]}>>(
        'cr-a11y-announcer-messages-sent', document.body);
    await dragHandleWithKeyboard(textbox, 'ArrowLeft', 5);
    event = await whenAnnounced;
    chrome.test.assertTrue(event.detail.messages.includes(
        loadTimeData.getString('ink2TextAnnotationMovedLeft')));

    // Move right via arrow keys.
    whenAnnounced = eventToPromise<CustomEvent<{messages: string[]}>>(
        'cr-a11y-announcer-messages-sent', document.body);
    await dragHandleWithKeyboard(textbox, 'ArrowRight', 5);
    event = await whenAnnounced;
    chrome.test.assertTrue(event.detail.messages.includes(
        loadTimeData.getString('ink2TextAnnotationMovedRight')));

    // Delete via keyboard shortcut.
    whenAnnounced = eventToPromise<CustomEvent<{messages: string[]}>>(
        'cr-a11y-announcer-messages-sent', document.body);
    keyDownOn(textbox, 0, [], 'Delete');
    event = await whenAnnounced;
    chrome.test.assertTrue(event.detail.messages.includes(
        loadTimeData.getString('ink2TextAnnotationDeleted')));

    chrome.test.succeed();
  },

  async function testCopyAnnotation() {
    const {textbox, manager, viewport, mockPlugin} = await setupTextBoxTest();
    const testAnnotation = await setUpExistingAnnotation(textbox, viewport);
    mockPlugin.clearMessages();

    // Trigger Copy shortcut (Ctrl+C / Cmd+C).
    const whenSaved = eventToPromise<
        CustomEvent<{annotation: TextAnnotation, isCut: boolean}>>(
        'saved-annotation-to-clipboard-for-testing', manager);
    keyDownOn(textbox, 0, getCtrlModifier(), 'c');
    const savedEvent = await whenSaved;

    // Verify copy calls into Ink2Manager with isCut = false.
    chrome.test.assertEq('Hello World', savedEvent.detail.annotation.text);
    chrome.test.assertFalse(savedEvent.detail.isCut);

    // Verify copy does not immediately commit; text box remains active.
    chrome.test.assertFalse(textbox.hidden);
    chrome.test.assertEq('Hello World', textbox.$.textbox.value);
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));

    // Modify text and commit. Verify updated annotation is committed.
    textbox.$.textbox.value = 'Updated Text';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await textbox.commitTextAnnotation();
    await microtasksFinished();

    chrome.test.assertTrue(textbox.hidden);
    const expectedAnnotation = {
      ...testAnnotation,
      text: 'Updated Text',
    };
    verifyFinishTextAnnotationMessage(
        mockPlugin, expectedAnnotation, /*expectedIsEdited=*/ true);

    chrome.test.succeed();
  },

  async function testCutAnnotation() {
    const {textbox, manager, viewport, mockPlugin} = await setupTextBoxTest();
    const testAnnotation = await setUpExistingAnnotation(textbox, viewport);
    mockPlugin.clearMessages();

    // Trigger Cut shortcut (Ctrl+X / Cmd+X).
    const whenSaved = eventToPromise<
        CustomEvent<{annotation: TextAnnotation, isCut: boolean}>>(
        'saved-annotation-to-clipboard-for-testing', manager);
    keyDownOn(textbox, 0, getCtrlModifier(), 'x');
    const savedEvent = await whenSaved;

    // Verify cut calls into Ink2Manager with isCut = true.
    chrome.test.assertEq('Hello World', savedEvent.detail.annotation.text);
    chrome.test.assertTrue(savedEvent.detail.isCut);

    await microtasksFinished();

    // Verify cut immediately deletes the annotation (commits empty).
    chrome.test.assertTrue(textbox.hidden);
    const expectedAnnotation = {
      ...testAnnotation,
      text: '',
    };
    verifyFinishTextAnnotationMessage(
        mockPlugin, expectedAnnotation, /*expectedIsEdited=*/ true);

    chrome.test.succeed();
  },

  async function testCopyAndCutIgnoredWhenTextSelected() {
    const {textbox, manager, viewport} = await setupTextBoxTest();
    await setUpExistingAnnotation(textbox, viewport);

    let savedEventFired = false;
    const listener = () => {
      savedEventFired = true;
    };
    manager.addEventListener(
        'saved-annotation-to-clipboard-for-testing', listener);

    // Ctrl+C on the textarea should not copy the annotation element.
    keyDownOn(textbox.$.textbox, 0, getCtrlModifier(), 'c');
    await microtasksFinished();
    chrome.test.assertFalse(savedEventFired);

    // Ctrl+X on the textarea should not cut the annotation element.
    keyDownOn(textbox.$.textbox, 0, getCtrlModifier(), 'x');
    await microtasksFinished();
    chrome.test.assertFalse(savedEventFired);
    chrome.test.assertFalse(textbox.hidden);

    manager.removeEventListener(
        'saved-annotation-to-clipboard-for-testing', listener);
    chrome.test.succeed();
  },

  async function testCopyIgnoredWhenInactive() {
    const {textbox, manager} = await setupTextBoxTest();
    chrome.test.assertTrue(textbox.hidden);

    let savedEventFired = false;
    const listener = () => {
      savedEventFired = true;
    };
    manager.addEventListener(
        'saved-annotation-to-clipboard-for-testing', listener);

    // Text box is INACTIVE. Copy should be ignored.
    keyDownOn(textbox, 0, getCtrlModifier(), 'c');
    await microtasksFinished();

    chrome.test.assertFalse(savedEventFired);

    manager.removeEventListener(
        'saved-annotation-to-clipboard-for-testing', listener);
    chrome.test.succeed();
  },

  async function testStylesShortcutsIgnoredWhenInactive() {
    const {textbox, manager} = await setupTextBoxTest();
    chrome.test.assertTrue(textbox.hidden);

    // Text box is INACTIVE. Shortcuts should be ignored.
    keyDownOn(textbox, 0, getCtrlModifier(), 'b');
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.BOLD]);

    keyDownOn(textbox, 0, getCtrlModifier(), 'i');
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.ITALIC]);

    keyDownOn(textbox, 0, getStrikethroughModifiers(), getStrikethroughKey());
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.STRIKETHROUGH]);

    chrome.test.succeed();
  },

  async function testBoldShortcut() {
    const {textbox, manager} = await setupTextBoxTest();
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();

    // Initial style is not bold.
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.BOLD]);

    // Ctrl+B on the textarea should toggle bold on.
    keyDownOn(textbox.$.textbox, 0, getCtrlModifier(), 'b');
    await microtasksFinished();
    chrome.test.assertTrue(
        manager.getCurrentTextAttributes().styles[TextStyle.BOLD]);

    // Ctrl+B on the textarea again should toggle bold off.
    keyDownOn(textbox.$.textbox, 0, getCtrlModifier(), 'b');
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.BOLD]);

    chrome.test.succeed();
  },

  async function testItalicShortcut() {
    const {textbox, manager} = await setupTextBoxTest();
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();

    // Initial style is not italic.
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.ITALIC]);

    // Ctrl+I on the textarea should toggle italic on.
    keyDownOn(textbox.$.textbox, 0, getCtrlModifier(), 'i');
    await microtasksFinished();
    chrome.test.assertTrue(
        manager.getCurrentTextAttributes().styles[TextStyle.ITALIC]);

    // Ctrl+I on the textarea again should toggle italic off.
    keyDownOn(textbox.$.textbox, 0, getCtrlModifier(), 'i');
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.ITALIC]);

    chrome.test.succeed();
  },

  async function testStrikethroughShortcut() {
    const {textbox, manager} = await setupTextBoxTest();
    initializeBox(100, 100, 400, 300);
    await microtasksFinished();

    // Initial style is not strikethrough.
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.STRIKETHROUGH]);

    // Strikethrough shortcut on the textarea should toggle strikethrough on.
    keyDownOn(
        textbox.$.textbox, 0, getStrikethroughModifiers(),
        getStrikethroughKey());
    await microtasksFinished();
    chrome.test.assertTrue(
        manager.getCurrentTextAttributes().styles[TextStyle.STRIKETHROUGH]);

    // Strikethrough shortcut on the textarea again should toggle strikethrough
    // off.
    keyDownOn(
        textbox.$.textbox, 0, getStrikethroughModifiers(),
        getStrikethroughKey());
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.STRIKETHROUGH]);

    chrome.test.succeed();
  },
]);
