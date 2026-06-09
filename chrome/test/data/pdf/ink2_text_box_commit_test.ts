// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DEFAULT_TEXTBOX_WIDTH, TextBoxState, TextStyle, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {TextAnnotation} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertPositionAndSize, dragHandle, getTestAnnotation, initializeBox, setupTextBoxTest, verifyFinishTextAnnotationMessage} from './ink2_text_box_test_utils.js';
import {assertDeepEquals} from './test_util.js';

const DEFAULT_HEIGHT = 24;
const FONT_SIZE = 12;
const CLICK_OFFSET = FONT_SIZE / 2;

chrome.test.runTests([
  async function testCommit() {
    const {manager, mockPlugin, textbox, viewport} = await setupTextBoxTest();
    // Initialize a new 100x100 box at 50, 60 (Box A).
    initializeBox(manager, 100, 100, 50, 60);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    // Set viewport to non-1.0 zoom to validate coordinate conversion.
    viewport.setZoom(2.0);
    await microtasksFinished();

    // With no edits, starting a new box B just deletes the existing Box A; the
    // plugin won't get a message.
    mockPlugin.clearMessages();
    // Initialize another new box.
    initializeBox(manager, 100, 100, 50, 60);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));

    // Helper to validate that creating a new annotation sends the expected
    // annotation information to the backend. Note that messages to the
    // backend are in page coordinates.
    async function startNewAnnotationAndVerifyMessage(
        expectedAnnotation: TextAnnotation, expectedIsEdited: boolean) {
      // Make sure that just applying edits hasn't triggered
      // finishTextAnnotation messages, and these are only triggered once the
      // user clicks elsewhere to create a new box.
      chrome.test.assertEq(
          undefined, mockPlugin.findMessage('finishTextAnnotation'));
      // Create a new box at 0, 0.
      initializeBox(manager, 100, 100, 0, 0);
      await microtasksFinished();
      verifyFinishTextAnnotationMessage(
          mockPlugin, expectedAnnotation, expectedIsEdited);
      mockPlugin.clearMessages();
    }

    // Commit the currently active new empty Box B by typing text into it
    // and then starting a new annotation (clicking elsewhere). This makes
    // it an "existing" annotation. Screen coordinates 50, 60 map to page
    // coordinates 25 - 5, 30 - 3. 5 and 3 are the page margin offsets.
    const expectedAnnotation = getTestAnnotation(
        {locationX: 20, locationY: 27, height: 50, width: 50}, 2.0);
    textbox.$.textbox.value = expectedAnnotation.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    await startNewAnnotationAndVerifyMessage(expectedAnnotation, true);

    // --- (1) Re-activate and edit the text of the existing annotation ---
    // Click center of Box B: [100, 110] screen to re-activate it.
    let clicked = await manager.initializeTextAnnotation({x: 100, y: 110});
    chrome.test.assertTrue(clicked, 'Failed to click existing annotation');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('Hello World', textbox.$.textbox.value);

    // Edit its text.
    const expectedAnnotationEditedText = structuredClone(expectedAnnotation);
    expectedAnnotationEditedText.text = 'Hello World Edited';
    textbox.$.textbox.value = expectedAnnotationEditedText.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Commit it by starting a new annotation and verify.
    await startNewAnnotationAndVerifyMessage(
        expectedAnnotationEditedText, true);

    // --- (2) Re-activate and apply Move/Resize Edit ---
    // Re-activate by clicking on it again (at [100, 110] screen).
    clicked = await manager.initializeTextAnnotation({x: 100, y: 110});
    chrome.test.assertTrue(clicked, 'Failed to click existing annotation');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('Hello World Edited', textbox.$.textbox.value);

    // Drag handle by 100px screen.
    await dragHandle(textbox, 100, 100);

    // Expected annotation after move: coordinates moved by 50px in page.
    const expectedAnnotationAfterMove =
        structuredClone(expectedAnnotationEditedText);
    expectedAnnotationAfterMove.textBoxRect = {
      locationX: 20 + 50,  // 70
      locationY: 27 + 50,  // 77
      width: 50,
      height: 50,
    };
    await startNewAnnotationAndVerifyMessage(expectedAnnotationAfterMove, true);

    // --- (3) Re-activate and apply Font Change Edit ---
    // Re-activate the moved annotation (new center is at [200, 210] screen).
    clicked = await manager.initializeTextAnnotation({x: 200, y: 210});
    chrome.test.assertTrue(clicked, 'Failed to click moved annotation');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // Apply font change.
    manager.setTextTypeface(TextTypeface.MONOSPACE);
    await microtasksFinished();
    const expectedAnnotationWithFont =
        structuredClone(expectedAnnotationAfterMove);
    expectedAnnotationWithFont.textAttributes.typeface = TextTypeface.MONOSPACE;
    await startNewAnnotationAndVerifyMessage(expectedAnnotationWithFont, true);

    // --- (4) Re-activate and apply Text Cleared Edit ---
    // Re-activate the font-changed annotation (same coordinates [200, 210]
    // screen).
    clicked = await manager.initializeTextAnnotation({x: 200, y: 210});
    chrome.test.assertTrue(clicked, 'Failed to click font-changed annotation');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // Clear the text.
    textbox.$.textbox.value = '';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    const expectedAnnotationTextCleared =
        structuredClone(expectedAnnotationWithFont);
    expectedAnnotationTextCleared.text = '';
    await startNewAnnotationAndVerifyMessage(
        expectedAnnotationTextCleared, true);
    chrome.test.succeed();
  },

  async function testCloseAndEvents() {
    const {manager, mockPlugin, textbox, viewport} = await setupTextBoxTest();
    viewport.setZoom(2.0);
    await microtasksFinished();
    // Add state-changed listener.
    let textBoxStates: TextBoxState[] = [];
    textbox.addEventListener('state-changed', e => {
      textBoxStates.push((e as CustomEvent<TextBoxState>).detail);
    });

    // Initialize to a 100x100 box at 400, 300.
    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    assertDeepEquals([TextBoxState.NEW], textBoxStates);

    // When a new box has no edits, commitTextAnnotation() will not trigger a
    // plugin message.
    mockPlugin.clearMessages();
    await textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));
    assertDeepEquals([TextBoxState.NEW, TextBoxState.INACTIVE], textBoxStates);

    // When text is edited, commitTextAnnotation() will trigger a plugin
    // message.
    textBoxStates = [];
    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    assertDeepEquals([TextBoxState.NEW], textBoxStates);
    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    assertDeepEquals([TextBoxState.NEW, TextBoxState.EDITED], textBoxStates);

    // Still using 2.0 zoom, so 100x100 at 400, 300 maps to 50x50 at
    // 400 / 2 - 5, 300 / 2 - 3 in page coordinates.
    const expectedAnnotation = getTestAnnotation(
        {locationX: 195, locationY: 147, height: 50, width: 50}, 2.0);
    expectedAnnotation.text = 'Hello';

    await textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    assertDeepEquals(
        [TextBoxState.NEW, TextBoxState.EDITED, TextBoxState.INACTIVE],
        textBoxStates);
    verifyFinishTextAnnotationMessage(mockPlugin, expectedAnnotation, true);
    mockPlugin.clearMessages();

    // When existing text is not edited, commitTextAnnotation() will trigger a
    // plugin message.
    textBoxStates = [];
    // Re-initialize the annotation.
    const clicked = await manager.initializeTextAnnotation({x: 450, y: 350});
    chrome.test.assertTrue(clicked, 'Failed to click existing annotation');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    assertDeepEquals([TextBoxState.NEW], textBoxStates);

    await textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    // The annotation has not changed.
    assertDeepEquals([TextBoxState.NEW, TextBoxState.INACTIVE], textBoxStates);
    verifyFinishTextAnnotationMessage(mockPlugin, expectedAnnotation, false);
    mockPlugin.clearMessages();

    chrome.test.succeed();
  },

  async function testCommitTextAnnotationFontCaching() {
    const {manager, privateProxy, textbox} = await setupTextBoxTest();
    privateProxy.reset();
    assertDeepEquals([], manager.getKnownFontIds());

    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    await textbox.commitTextAnnotation();

    let getTextInfoArgs = await privateProxy.whenCalled('getTextInfo');
    assertDeepEquals([], getTextInfoArgs.knownFontIds);
    chrome.test.assertEq(textbox.$.textbox, getTextInfoArgs.textarea);

    // Test with some text information.
    privateProxy.reset();
    privateProxy.setGetTextInfoResult({
      typefaces: [{uniqueId: 123, serializedTypeface: new ArrayBuffer(10)}],
      mojoTextInfo: new ArrayBuffer(5),
    });

    initializeBox(manager, 100, 100, 50, 50);
    await microtasksFinished();
    textbox.$.textbox.value = 'World';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    await textbox.commitTextAnnotation();
    getTextInfoArgs = await privateProxy.whenCalled('getTextInfo');
    assertDeepEquals([], getTextInfoArgs.knownFontIds);
    assertDeepEquals([123], manager.getKnownFontIds());

    // Test with the same text information.
    privateProxy.reset();
    // In production, the proxy is responsible for never sending the same
    // typeface twice.
    privateProxy.setGetTextInfoResult({
      typefaces: [],
      mojoTextInfo: new ArrayBuffer(5),
    });
    initializeBox(manager, 100, 100, 200, 150);
    await microtasksFinished();
    textbox.$.textbox.value = 'Again';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    await textbox.commitTextAnnotation();
    getTextInfoArgs = await privateProxy.whenCalled('getTextInfo');
    // The text box is responsible for telling the proxy 123 is a known id.
    assertDeepEquals([123], getTextInfoArgs.knownFontIds);

    chrome.test.succeed();
  },

  // Regression test for crbug.com/519251246
  async function testClickOldPositionAfterMove() {
    const {manager, mockPlugin, textbox, viewport} = await setupTextBoxTest();

    // The textbox uses screen coordinates, but the CSS styles apply offsets.
    // X offset: --offset-x (17px)
    // Y offset: --offset-y (15px)
    // Width offset: border (2 * 1px) + padding (2 * 6px) + X offsets (2 * 5px)
    // = 24px
    // Height offset: border (2 * 1px) + padding (2 * 4px) + Y offsets (2 * 5px)
    // = 20px
    // Matched to chrome/browser/resources/pdf/elements/ink_text_box.css styles.
    const OFFSET_X = 17;
    const OFFSET_Y = 15;
    const WIDTH_OFFSET = 24;
    const HEIGHT_OFFSET = 20;

    const pageRect = viewport.getPageScreenRect(0);

    // (1) Create Box A at 95, 60 in screen coordinates (40, 57 in page
    // coordinates). Since the current text size is 12, the y position of the
    // click is offset by 12 / 2 = 6, resulting in a click at 95, 66 screen (40,
    // 63 page).
    const clickXA = 95;
    const clickYA = 66;
    const createdA =
        await manager.initializeTextAnnotation({x: clickXA, y: clickYA});
    chrome.test.assertTrue(createdA, 'Failed to initialize Box A');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // locationX_ is set to pageX (40).
    // locationY_ is set to pageY - clickOffset = 63 - 6 = 60.
    // Left: locationX_ (95) - OFFSET_X (17) = 78px.
    // Top: locationY_ (60) - OFFSET_Y (15) = 45px.
    let expectedLeft = clickXA - OFFSET_X;
    let expectedTop = clickYA - CLICK_OFFSET - OFFSET_Y;
    const expectedWidthStyle = `${DEFAULT_TEXTBOX_WIDTH + WIDTH_OFFSET}px`;
    const expectedHeightStyle = `${DEFAULT_HEIGHT + HEIGHT_OFFSET}px`;
    assertPositionAndSize(
        textbox, expectedWidthStyle, expectedHeightStyle, `${expectedLeft}px`,
        `${expectedTop}px`);

    textbox.$.textbox.value = 'Annotation A';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // (2) Move it to a new position by dragging 100px in both directions.
    await dragHandle(textbox, 100, 100);
    expectedLeft += 100;
    expectedTop += 100;

    // New screen coordinates: x = 95 + 100 = 195, y = 60 + 100 = 160.
    // Left: 195 - OFFSET_X (17) = 178px. Top: 160 - OFFSET_Y (15) = 145px.
    assertPositionAndSize(
        textbox, expectedWidthStyle, expectedHeightStyle, `${expectedLeft}px`,
        `${expectedTop}px`);

    // (3) Initialize a new box (Box B) using the manager with a click position
    // in the original center of Box A.
    // Center X = locationX_ (95) + DEFAULT_TEXTBOX_WIDTH (222) / 2 = 206.
    // Center Y = locationY_ (60) + DEFAULT_HEIGHT (24) / 2 = 72.
    const clickXB = clickXA + DEFAULT_TEXTBOX_WIDTH / 2;
    const clickYB = (clickYA - CLICK_OFFSET) + DEFAULT_HEIGHT / 2;

    mockPlugin.clearMessages();
    const createdB =
        await manager.initializeTextAnnotation({x: clickXB, y: clickYB});
    chrome.test.assertTrue(createdB, 'Failed to initialize Box B');
    await microtasksFinished();

    // (4) Validate that the ink-text-box commits Box A at the moved position.
    // Moved page coordinates:
    // locationX = (195 (screenX) - 55 (pageRect.x)) / 1.0 = 140.
    // locationY = (160 (screenY) - 3 (pageRect.y)) / 1.0 = 157.
    const expectedAnnotationA = getTestAnnotation({
      locationX: clickXA + 100 - pageRect.x,
      locationY: clickYA - CLICK_OFFSET + 100 - pageRect.y,
      width: DEFAULT_TEXTBOX_WIDTH,
      height: DEFAULT_HEIGHT,
    });
    expectedAnnotationA.text = 'Annotation A';
    expectedAnnotationA.id = 0;
    verifyFinishTextAnnotationMessage(mockPlugin, expectedAnnotationA, true);
    mockPlugin.clearMessages();

    // (5) Validate that the ink-text-box moves to the new position of Box B,
    // and shows empty text.
    // Box B screen coordinates:
    // x = 206 -> locationX_ = 206.
    // y = 72 - 6 (clickOffset) = 66 -> locationY_ = 66.
    // Left: 206 - OFFSET_X (17) = 189px. Top: 66 - OFFSET_Y (15) = 51px.
    const expectedLeftB = clickXB - OFFSET_X;
    const expectedTopB = clickYB - CLICK_OFFSET - OFFSET_Y;
    assertPositionAndSize(
        textbox, expectedWidthStyle, expectedHeightStyle, `${expectedLeftB}px`,
        `${expectedTopB}px`);
    chrome.test.assertEq('', textbox.$.textbox.value);

    // (6) Enter some text in Box B.
    textbox.$.textbox.value = 'Annotation B';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // (7) Validate that deactivating the text box commits Box B with a
    // different ID and text content from Box A.
    const stateChanged = eventToPromise('state-changed', textbox);
    manager.dispatchEvent(new CustomEvent('deactivate-text-box'));
    await stateChanged;
    await microtasksFinished();

    // Committed page coordinates for Box B:
    // locationX = (206 (screenX) - 55 (pageRect.x)) / 1.0 = 151.
    // locationY = (66 (screenY) - 3 (pageRect.y)) / 1.0 = 63.
    const expectedAnnotationB = getTestAnnotation({
      locationX: clickXB - pageRect.x,
      locationY: clickYB - CLICK_OFFSET - pageRect.y,
      width: DEFAULT_TEXTBOX_WIDTH,
      height: DEFAULT_HEIGHT,
    });
    expectedAnnotationB.text = 'Annotation B';
    expectedAnnotationB.id = 1;
    verifyFinishTextAnnotationMessage(mockPlugin, expectedAnnotationB, true);

    chrome.test.succeed();
  },

  // Tests that when an existing annotation is re-styled, the new styles are
  // correctly committed when a new annotation is activated.
  // Regression test for crbug.com/519251247
  async function testCommitStyleUpdates() {
    const {manager, mockPlugin, textbox, viewport} = await setupTextBoxTest();
    const pageRect = viewport.getPageScreenRect(0);

    // (1) Initialize a text annotation. Click at screen coordinates 100, 66
    // which locates the annotation at 100, 60 due to the click offset.
    const clickXA = 100;
    const clickYA = 66;
    const createdA =
        await manager.initializeTextAnnotation({x: clickXA, y: clickYA});
    chrome.test.assertTrue(createdA, 'Failed to initialize Box A');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // (2) Add some text.
    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // (3) Commit the annotation and validate.
    const expectedAnnotation = getTestAnnotation({
      locationX: clickXA - pageRect.x,
      locationY: clickYA - CLICK_OFFSET - pageRect.y,
      width: DEFAULT_TEXTBOX_WIDTH,
      height: DEFAULT_HEIGHT,
    });
    expectedAnnotation.text = 'Hello';
    expectedAnnotation.id = 0;

    await textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    verifyFinishTextAnnotationMessage(mockPlugin, expectedAnnotation, true);
    mockPlugin.clearMessages();

    // (4) Re-activate this annotation.
    // Center X = 100 + DEFAULT_TEXTBOX_WIDTH / 2 = 211.
    // Center Y = 60 + DEFAULT_HEIGHT / 2 = 72.
    const clickXReactivate = clickXA + DEFAULT_TEXTBOX_WIDTH / 2;
    const clickYReactivate = (clickYA - CLICK_OFFSET) + DEFAULT_HEIGHT / 2;
    const clicked = await manager.initializeTextAnnotation(
        {x: clickXReactivate, y: clickYReactivate});
    chrome.test.assertTrue(clicked, 'Failed to click existing annotation');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('Hello', textbox.$.textbox.value);

    // (5) Modify the font color and style to red + bold.
    manager.setTextColor({r: 255, g: 0, b: 0});
    manager.setTextStyles({
      [TextStyle.BOLD]: true,
      [TextStyle.ITALIC]: false,
    });
    await microtasksFinished();

    // (6) Initialize a new annotation elsewhere.
    // Click at 200, 206 screen -> top-left at 200, 200 screen after offset.
    const createdB = await manager.initializeTextAnnotation({x: 200, y: 206});
    chrome.test.assertTrue(createdB, 'Failed to initialize Box B');
    await microtasksFinished();

    // (7) Validate that the commit message triggered by the new initialization
    // contains the correct styling.
    const expectedAnnotationEdited = structuredClone(expectedAnnotation);
    expectedAnnotationEdited.textAttributes.color = {r: 255, g: 0, b: 0};
    expectedAnnotationEdited.textAttributes.styles[TextStyle.BOLD] = true;
    expectedAnnotationEdited.textAttributes.styles[TextStyle.ITALIC] = false;

    verifyFinishTextAnnotationMessage(
        mockPlugin, expectedAnnotationEdited, true);

    chrome.test.succeed();
  },
]);
