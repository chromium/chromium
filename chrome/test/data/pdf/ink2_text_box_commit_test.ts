// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TextBoxState, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {TextAnnotation} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {dragHandle, getTestAnnotation, initializeBox, setupTextBoxTest, verifyFinishTextAnnotationMessage} from './ink2_text_box_test_utils.js';
import {assertDeepEquals} from './test_util.js';

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
]);
