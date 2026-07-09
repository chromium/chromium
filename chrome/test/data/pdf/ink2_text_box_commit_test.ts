// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TextBoxState, TextTypeface, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {TextAnnotation} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {dragHandle, getTestAnnotation, initializeBox, reactivateBox, setupTextBoxTest, verifyFinishTextAnnotationMessage} from './ink2_text_box_test_utils.js';
import {assertDeepEquals, setupMockMetricsPrivate} from './test_util.js';

chrome.test.runTests([
  async function testCommit() {
    const mockMetricsPrivate = setupMockMetricsPrivate();
    const {manager, mockPlugin, textbox, viewport} = await setupTextBoxTest();
    // Initialize a new 100x100 box at 50, 60 (Box A).
    initializeBox(100, 100, 50, 60);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    // Set viewport to non-1.0 zoom to validate coordinate conversion.
    viewport.setZoom(2.0);
    await microtasksFinished();

    // With no edits, starting a new box B just deletes the existing Box A; the
    // plugin won't get a message.
    mockPlugin.clearMessages();
    // Initialize another new box.
    initializeBox(100, 100, 50, 60);
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
      // Manually commit the active annotation.
      await textbox.commitTextAnnotation();
      // Create a new box at 0, 0.
      initializeBox(100, 100, 0, 0);
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
    mockMetricsPrivate.assertCount(UserAction.ADD_INK2_TEXT_ANNOTATION, 1);

    // --- (1) Re-activate and edit the text of the existing annotation ---
    reactivateBox(textbox, viewport, expectedAnnotation);
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
    mockMetricsPrivate.assertCount(UserAction.ADD_INK2_TEXT_ANNOTATION, 1);

    // --- (2) Re-activate and apply Move/Resize Edit ---
    reactivateBox(textbox, viewport, expectedAnnotationEditedText);
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
    reactivateBox(textbox, viewport, expectedAnnotationAfterMove);
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
    reactivateBox(textbox, viewport, expectedAnnotationWithFont);
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
    const mockMetricsPrivate = setupMockMetricsPrivate();
    const {mockPlugin, textbox, viewport} = await setupTextBoxTest();
    viewport.setZoom(2.0);
    await microtasksFinished();
    // Add state-changed listener.
    let textBoxStates: TextBoxState[] = [];
    textbox.addEventListener('state-changed', e => {
      textBoxStates.push((e as CustomEvent<TextBoxState>).detail);
    });

    // Initialize to a 100x100 box at 400, 300.
    initializeBox(100, 100, 400, 300);
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
    // No edits also does not count as an add event in the metrics.
    mockMetricsPrivate.assertCount(UserAction.ADD_INK2_TEXT_ANNOTATION, 0);

    // When text is edited, commitTextAnnotation() will trigger a plugin
    // message.
    textBoxStates = [];
    initializeBox(100, 100, 400, 300);
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
    mockMetricsPrivate.assertCount(UserAction.ADD_INK2_TEXT_ANNOTATION, 1);
    mockPlugin.clearMessages();

    // When existing text is not edited, commitTextAnnotation() will trigger a
    // plugin message.
    textBoxStates = [];
    // Re-initialize the annotation.
    reactivateBox(textbox, viewport, expectedAnnotation);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    assertDeepEquals([TextBoxState.NEW], textBoxStates);

    await textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    // The annotation has not changed.
    assertDeepEquals([TextBoxState.NEW, TextBoxState.INACTIVE], textBoxStates);
    verifyFinishTextAnnotationMessage(mockPlugin, expectedAnnotation, false);
    // No edits also does not count as an add event in the metrics. So the count
    // stays at 1 here.
    mockMetricsPrivate.assertCount(UserAction.ADD_INK2_TEXT_ANNOTATION, 1);
    mockPlugin.clearMessages();

    chrome.test.succeed();
  },

  async function testCommitTextAnnotationFontCaching() {
    const {manager, privateProxy, textbox} = await setupTextBoxTest();
    privateProxy.reset();
    assertDeepEquals([], manager.getKnownFontIds());

    initializeBox(100, 100, 400, 300);
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

    initializeBox(100, 100, 50, 50);
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
    initializeBox(100, 100, 200, 150);
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

  async function testCommitNoEditsDoesNotFetchFont() {
    const {manager, privateProxy, textbox, viewport} = await setupTextBoxTest();
    assertDeepEquals([], manager.getKnownFontIds());

    // Set up test font information.
    privateProxy.reset();
    privateProxy.setGetTextInfoResult({
      typefaces: [{uniqueId: 123, serializedTypeface: new ArrayBuffer(10)}],
      mojoTextInfo: new ArrayBuffer(5),
    });

    // Simulate opening a loaded text annotation by reactivating the annotation.
    const loadedAnnotation = getTestAnnotation(
        {locationX: 20, locationY: 27, height: 50, width: 50}, 1.0);
    reactivateBox(textbox, viewport, loadedAnnotation);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // Commit without making any edits.
    await textbox.commitTextAnnotation();
    await microtasksFinished();

    chrome.test.assertEq(0, privateProxy.getCallCount('getTextInfo'));
    assertDeepEquals([], manager.getKnownFontIds());

    // Initialize a new box.
    initializeBox(100, 100, 50, 50);
    await microtasksFinished();

    // Type some text to make it edited.
    textbox.$.textbox.value = 'New Annotation';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Commit the new annotation.
    await textbox.commitTextAnnotation();
    await microtasksFinished();

    // Verify that getTextInfo() was called and a new font was collected.
    chrome.test.assertEq(1, privateProxy.getCallCount('getTextInfo'));
    const getTextInfoArgs = await privateProxy.whenCalled('getTextInfo');
    assertDeepEquals([], getTextInfoArgs.knownFontIds);
    assertDeepEquals([123], manager.getKnownFontIds());

    chrome.test.succeed();
  },
]);
