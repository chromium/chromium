// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, Ink2Manager, PdfViewerPrivateProxyImpl, PluginController, PluginControllerEventType, TextStyle} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getCtrlModifier, getStrikethroughKey, getStrikethroughModifiers} from './ink2_text_box_test_utils.js';
import {getNewTestBeforeUnloadProxy} from './test_before_unload_proxy.js';
import {TestPdfViewerPrivateProxy} from './test_pdf_viewer_private_proxy.js';
import {createTextBox, getRequiredElement, getTextBox, setupMockMetricsPrivate, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
const mockPlugin = setupTestMockPluginForInk();
const mockMetricsPrivate = setupMockMetricsPrivate();

const privateProxy = new TestPdfViewerPrivateProxy();
PdfViewerPrivateProxyImpl.setInstance(privateProxy);

// Disable beforeunload to avoid hanging after tests succeed.
getNewTestBeforeUnloadProxy();

async function setAnnotationMode(mode: AnnotationMode) {
  viewerToolbar.setAnnotationMode(mode);
  await microtasksFinished();
}

function dispatchSendClickEvent(x: number = 50, y: number = 50) {
  PluginController.getInstance().getEventTarget().dispatchEvent(new CustomEvent(
      PluginControllerEventType.PLUGIN_MESSAGE,
      {detail: {type: 'sendClickEvent', x, y}}));
}

chrome.test.runTests([
  async function testTextboxVisibility() {
    // Annotation mode off. Textbox is not in the DOM.
    await setAnnotationMode(AnnotationMode.OFF);
    let textbox = getTextBox(viewer);
    chrome.test.assertFalse(!!textbox);

    // Text annotation mode. Textbox is in the DOM but isn't visible.
    await setAnnotationMode(AnnotationMode.TEXT);
    textbox = getTextBox(viewer);
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertFalse(isVisible(textbox));

    // Simulate clicking the plugin. pdf-viewer should notify Ink2Manager to
    // initialize an annotation, which shows the box.
    dispatchSendClickEvent();
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // Switching to a different annotation mode removes the box from the DOM.
    await setAnnotationMode(AnnotationMode.DRAW);
    textbox = getTextBox(viewer);
    chrome.test.assertFalse(!!textbox);

    // Text annotation mode. Switching back to text puts the box back in the
    // DOM, but does not immediately make it visible.
    await setAnnotationMode(AnnotationMode.TEXT);
    textbox = getTextBox(viewer);
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertFalse(isVisible(textbox));

    chrome.test.succeed();
  },

  async function testSwitchFromTextToDrawCommits() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Switch to TEXT mode.
    await setAnnotationMode(AnnotationMode.TEXT);
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);

    // Create a textbox.
    createTextBox();
    await microtasksFinished();
    const textbox = getTextBox(viewer);
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertTrue(isVisible(textbox));

    // Edit the textbox.
    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Switch to DRAW mode. This should commit the textbox.
    await setAnnotationMode(AnnotationMode.DRAW);

    // Verify textbox is removed and committed.
    chrome.test.assertFalse(!!getTextBox(viewer));
    const finishMessage = mockPlugin.findMessage('finishTextAnnotation');
    chrome.test.assertNe(undefined, finishMessage);
    chrome.test.succeed();
  },

  async function testSwitchFromTextToOffCommits() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Switch to TEXT mode.
    await setAnnotationMode(AnnotationMode.TEXT);

    // Create a textbox.
    createTextBox();
    await microtasksFinished();
    const textbox = getTextBox(viewer)!;

    // Edit the textbox.
    textbox.$.textbox.value = 'World';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Switch to OFF mode. This should commit the textbox.
    await setAnnotationMode(AnnotationMode.OFF);

    // Verify textbox is removed and committed.
    chrome.test.assertFalse(!!getTextBox(viewer));
    const finishMessage = mockPlugin.findMessage('finishTextAnnotation');
    chrome.test.assertNe(undefined, finishMessage);
    chrome.test.succeed();
  },

  async function testPrintCommits() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Enable printing.
    loadTimeData.overrideValues({
      'printingEnabled': true,
    });
    viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
    await microtasksFinished();

    // Switch to TEXT mode.
    await setAnnotationMode(AnnotationMode.TEXT);

    // Create a textbox.
    createTextBox();
    await microtasksFinished();
    const textbox = getTextBox(viewer)!;

    // Edit the textbox.
    textbox.$.textbox.value = 'Print me';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Trigger print via toolbar button.
    const printButton =
        viewerToolbar.shadowRoot.querySelector<HTMLElement>('#print')!;
    chrome.test.assertTrue(!!printButton);
    printButton.click();
    await microtasksFinished();

    // The finishTextAnnotation message should have been sent before print.
    const setTextIndex = mockPlugin.messages.findIndex(
        message => message.type === 'finishTextAnnotation');
    const printIndex =
        mockPlugin.messages.findIndex(message => message.type === 'print');
    chrome.test.assertNe(-1, setTextIndex);
    chrome.test.assertNe(-1, printIndex);
    chrome.test.assertTrue(setTextIndex < printIndex);

    chrome.test.succeed();
  },

  async function testScriptPrintCommits() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Enable printing.
    loadTimeData.overrideValues({
      'printingEnabled': true,
    });
    viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
    await microtasksFinished();

    // Switch to TEXT mode.
    await setAnnotationMode(AnnotationMode.TEXT);

    // Create a textbox.
    createTextBox();
    await microtasksFinished();
    const textbox = getTextBox(viewer)!;

    // Edit the textbox.
    textbox.$.textbox.value = 'Print me script';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Trigger print via scripting API.
    viewer.handleScriptingMessage(new MessageEvent('message', {
      data: {type: 'print'},
    }));
    await microtasksFinished();

    // The finishTextAnnotation message should have been sent before print.
    const setTextIndex = mockPlugin.messages.findIndex(
        message => message.type === 'finishTextAnnotation');
    const printIndex =
        mockPlugin.messages.findIndex(message => message.type === 'print');
    chrome.test.assertNe(-1, setTextIndex);
    chrome.test.assertNe(-1, printIndex);
    chrome.test.assertTrue(setTextIndex < printIndex);

    chrome.test.succeed();
  },

  async function testFocusAfterCommit() {
    mockPlugin.clearMessages();
    await setAnnotationMode(AnnotationMode.TEXT);

    createTextBox();
    await microtasksFinished();
    const textbox = getTextBox(viewer);
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertTrue(isVisible(textbox));

    // Edit the textbox.
    const textarea = textbox.$.textbox;
    textarea.value = 'Focus test';
    textarea.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Press Escape to focus the outer box.
    const escapePromise =
        eventToPromise('ink-text-box-focused-for-test', textbox);
    textarea.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Escape', bubbles: true, composed: true}));
    await escapePromise;

    // Verify textbox has focus.
    const annotations = getRequiredElement(viewer, 'ink-text-annotations');
    chrome.test.assertEq(textbox, annotations.shadowRoot.activeElement);

    // Press Escape again to commit.
    document.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'Escape', bubbles: true, composed: true}));
    await microtasksFinished();

    // Verify textbox is committed, hidden, and no longer has focus.
    chrome.test.assertFalse(isVisible(textbox));
    chrome.test.assertNe(textbox, annotations.shadowRoot.activeElement);

    // Verify that the PDF content embed now has focus.
    const embed = getRequiredElement(viewer, 'embed');
    chrome.test.assertEq(embed, viewer.shadowRoot.activeElement);

    chrome.test.succeed();
  },

  async function testCommitOnClick() {
    mockPlugin.clearMessages();
    await setAnnotationMode(AnnotationMode.TEXT);

    // Click to create first textbox.
    dispatchSendClickEvent();
    await microtasksFinished();
    const textbox = getTextBox(viewer);
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertTrue(isVisible(textbox));
    // Edit textbox value.
    textbox.$.textbox.value = 'Click Outside Test';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Click outside the current textbox to commit it.
    const whenStateChanged = eventToPromise('state-changed', textbox);
    dispatchSendClickEvent(350, 50);
    await whenStateChanged;
    await microtasksFinished();

    // Verify textbox was committed and hidden.
    chrome.test.assertFalse(isVisible(textbox));
    const finishMessage = mockPlugin.findMessage('finishTextAnnotation');
    chrome.test.assertNe(undefined, finishMessage);
    mockPlugin.clearMessages();

    // Click again at a location outside the committed annotation.
    // This should create a new textbox.
    dispatchSendClickEvent(350, 50);
    await microtasksFinished();

    // Empty new textbox is visible.
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('', textbox.$.textbox.value);

    chrome.test.succeed();
  },

  // Test bold, italic, strikethrough, and underline keyboard shortcuts toggle
  // styles in text mode.
  async function testTextAnnotationStyleKeyboardShortcuts() {
    await setAnnotationMode(AnnotationMode.OFF);
    const manager = Ink2Manager.getInstance();

    // Shortcuts are ignored when not in text annotation mode.
    keyDownOn(viewer, 0, getCtrlModifier(), 'b');
    keyDownOn(viewer, 0, getCtrlModifier(), 'i');
    keyDownOn(viewer, 0, getStrikethroughModifiers(), getStrikethroughKey());
    keyDownOn(viewer, 0, getCtrlModifier(), 'u');
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.BOLD]);
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.ITALIC]);
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.STRIKETHROUGH]);
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.UNDERLINE]);

    // Enable text annotation mode.
    await setAnnotationMode(AnnotationMode.TEXT);

    // Toggle bold on and off.
    keyDownOn(viewer, 0, getCtrlModifier(), 'b');
    await microtasksFinished();
    chrome.test.assertTrue(
        manager.getCurrentTextAttributes().styles[TextStyle.BOLD]);

    keyDownOn(viewer, 0, getCtrlModifier(), 'b');
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.BOLD]);

    // Toggle italic on and off.
    keyDownOn(viewer, 0, getCtrlModifier(), 'i');
    await microtasksFinished();
    chrome.test.assertTrue(
        manager.getCurrentTextAttributes().styles[TextStyle.ITALIC]);

    keyDownOn(viewer, 0, getCtrlModifier(), 'i');
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.ITALIC]);

    // Toggle strikethrough on and off.
    keyDownOn(viewer, 0, getStrikethroughModifiers(), getStrikethroughKey());
    await microtasksFinished();
    chrome.test.assertTrue(
        manager.getCurrentTextAttributes().styles[TextStyle.STRIKETHROUGH]);

    keyDownOn(viewer, 0, getStrikethroughModifiers(), getStrikethroughKey());
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.STRIKETHROUGH]);

    // Toggle underline on and off.
    keyDownOn(viewer, 0, getCtrlModifier(), 'u');
    await microtasksFinished();
    chrome.test.assertTrue(
        manager.getCurrentTextAttributes().styles[TextStyle.UNDERLINE]);

    keyDownOn(viewer, 0, getCtrlModifier(), 'u');
    await microtasksFinished();
    chrome.test.assertFalse(
        manager.getCurrentTextAttributes().styles[TextStyle.UNDERLINE]);

    chrome.test.succeed();
  },
]);
