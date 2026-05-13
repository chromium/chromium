// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TextBoxState, TextTypeface} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {dragHandle, getTestAnnotation, initializeBox, setupTextBoxTest, verifyFinishTextAnnotationMessage} from './ink2_text_box_test_utils.js';
import {assertDeepEquals} from './test_util.js';

const {manager, mockPlugin, privateProxy, textbox, viewport} =
    setupTextBoxTest();

chrome.test.runTests([
  async function testCommit() {
    // Initialize to a 100x100 box at 400, 300.
    initializeBox(manager, 100, 100, 400, 300);
    chrome.test.assertTrue(isVisible(textbox));
    await microtasksFinished();
    // Reset viewport to less offset page values and a non-1.0 zoom to validate
    // coordinate conversion.
    viewport.setZoom(2.0);
    await microtasksFinished();

    // With no edits, starting a new box just deletes the existing one; the
    // plugin won't get a message.
    mockPlugin.clearMessages();
    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));

    // Editing text --> commit annotation on event.
    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    // Messages to the backend are in page coordinates.
    const testAnnotation = getTestAnnotation(
        {locationX: 195, locationY: 147, height: 50, width: 50});

    function startNewAnnotationAndVerifyMessage(
        expectedIsEdited: boolean, existing: boolean = false) {
      mockPlugin.clearMessages();
      initializeBox(manager, 100, 100, 400, 300, existing);
      verifyFinishTextAnnotationMessage(
          mockPlugin, testAnnotation, expectedIsEdited, 2.0);
    }

    textbox.$.textbox.value = testAnnotation.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    startNewAnnotationAndVerifyMessage(true);
    await microtasksFinished();

    // Moving (or resizing) the box is an edit. Also need to input some text,
    // as empty annotations are ignored.
    chrome.test.assertTrue(isVisible(textbox));
    textbox.$.textbox.value = testAnnotation.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    await dragHandle(textbox, 100, 100);
    // Adjust expectations for new box. Text is reset.
    // At 2x zoom, a 100px move in screen coordinates is a 50px move in page
    // coordinates.
    testAnnotation
        .textBoxRect = {height: 50, width: 50, locationX: 245, locationY: 197};
    startNewAnnotationAndVerifyMessage(true);
    await microtasksFinished();
    // Reset expectation.
    testAnnotation
        .textBoxRect = {height: 50, width: 50, locationX: 195, locationY: 147};

    // Any modifications to font are an edit.
    chrome.test.assertTrue(isVisible(textbox));
    textbox.$.textbox.value = testAnnotation.text;
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    manager.setTextTypeface(TextTypeface.MONOSPACE);
    await microtasksFinished();
    testAnnotation.textAttributes.typeface = TextTypeface.MONOSPACE;
    startNewAnnotationAndVerifyMessage(true);
    await microtasksFinished();
    // Reset expectation.
    testAnnotation.textAttributes.typeface = TextTypeface.SANS_SERIF;

    // If all the text is deleted, there is also no commit message.
    chrome.test.assertTrue(isVisible(textbox));
    textbox.$.textbox.value = '';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    mockPlugin.clearMessages();
    // Initialize an existing box to set up the next test.
    initializeBox(manager, 100, 100, 400, 300, true);
    await microtasksFinished();
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('finishTextAnnotation'));

    // If we are editing an existing box, a finish message should be sent
    // regardless of edits or text.
    chrome.test.assertTrue(isVisible(textbox));
    testAnnotation.text = 'Hello World';
    startNewAnnotationAndVerifyMessage(false, /* existing= */ true);
    await microtasksFinished();

    // Existing box, text cleared.
    chrome.test.assertTrue(isVisible(textbox));
    textbox.$.textbox.value = '';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();
    testAnnotation.text = '';
    startNewAnnotationAndVerifyMessage(true, /* existing= */ true);
    await microtasksFinished();

    // Message should also be sent if the element is disconnected.
    chrome.test.assertTrue(isVisible(textbox));
    testAnnotation.text = 'Hello World';
    await microtasksFinished();
    mockPlugin.clearMessages();
    // This happens if the user changes annotation mode.
    textbox.remove();
    verifyFinishTextAnnotationMessage(mockPlugin, testAnnotation, false, 2.0);

    // Reset for future tests.
    document.body.appendChild(textbox);

    chrome.test.succeed();
  },

  async function testCloseAndEvents() {
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
    textbox.commitTextAnnotation();
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

    const expectedEditedAnnotation = getTestAnnotation(
        {locationX: 400, locationY: 300, height: 100, width: 100});
    expectedEditedAnnotation.text = 'Hello';

    textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    verifyFinishTextAnnotationMessage(
        mockPlugin, expectedEditedAnnotation, true, 1.0);
    assertDeepEquals(
        [TextBoxState.NEW, TextBoxState.EDITED, TextBoxState.INACTIVE],
        textBoxStates);

    // When existing text is not edited, commitTextAnnotation() will trigger a
    // plugin message.
    textBoxStates = [];
    initializeBox(manager, 100, 100, 400, 300, true);
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    assertDeepEquals([TextBoxState.NEW], textBoxStates);

    const expectedUneditedAnnotation = getTestAnnotation(
        {locationX: 400, locationY: 300, height: 100, width: 100});

    textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    verifyFinishTextAnnotationMessage(
        mockPlugin, expectedUneditedAnnotation, false, 1.0);
    assertDeepEquals([TextBoxState.NEW, TextBoxState.INACTIVE], textBoxStates);

    chrome.test.succeed();
  },

  async function testCommitTextAnnotationFontCaching() {
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

    privateProxy.reset();
    privateProxy.setGetTextInfoResult({
      typefaces: [{uniqueId: 123, serializedTypeface: new ArrayBuffer(10)}],
      mojoTextInfo: new ArrayBuffer(5),
    });

    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    textbox.$.textbox.value = 'World';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    await textbox.commitTextAnnotation();

    getTextInfoArgs = await privateProxy.whenCalled('getTextInfo');
    assertDeepEquals([], getTextInfoArgs.knownFontIds);
    assertDeepEquals([123], manager.getKnownFontIds());

    privateProxy.reset();
    initializeBox(manager, 100, 100, 400, 300);
    await microtasksFinished();
    textbox.$.textbox.value = 'Again';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    await textbox.commitTextAnnotation();

    getTextInfoArgs = await privateProxy.whenCalled('getTextInfo');
    assertDeepEquals([123], getTextInfoArgs.knownFontIds);

    chrome.test.succeed();
  },
]);
