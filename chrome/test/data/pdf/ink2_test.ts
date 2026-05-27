// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, PluginController, PluginControllerEventType, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getNewTestBeforeUnloadProxy} from './test_before_unload_proxy.js';
import {createTextBox, setupMockMetricsPrivate, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
const mockPlugin = setupTestMockPluginForInk();
const mockMetricsPrivate = setupMockMetricsPrivate();

// Disable beforeunload to avoid hanging after tests succeed.
getNewTestBeforeUnloadProxy();

async function enableTextAnnotations(enabled: boolean) {
  loadTimeData.overrideValues({'pdfTextAnnotationsEnabled': enabled});
  viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
  await microtasksFinished();
}

async function setAnnotationMode(mode: AnnotationMode) {
  viewerToolbar.setAnnotationMode(mode);
  await microtasksFinished();
}

chrome.test.runTests([
  // Test that Ink2 mode are enabled.
  function testAnnotationsEnabled() {
    chrome.test.assertTrue(loadTimeData.getBoolean('pdfInk2Enabled'));
    // When ink2 and annotations are enabled in loadTimeData, the ink2
    // button section displays.
    chrome.test.assertTrue(
        !!viewerToolbar.shadowRoot.querySelector('#annotate-controls'));
    chrome.test.succeed();
  },

  // Test that annotation mode can be set.
  // TODO (crbug.com/402547554): Remove this test once text annotations
  // launches, since it will be fully covered by the test below.
  async function testSetAnnotationMode() {
    mockMetricsPrivate.reset();

    // Disable text annotations.
    await enableTextAnnotations(false);
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    await setAnnotationMode(AnnotationMode.DRAW);
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    mockMetricsPrivate.assertCount(UserAction.ENTER_INK2_ANNOTATION_MODE, 1);

    await setAnnotationMode(AnnotationMode.OFF);
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    mockMetricsPrivate.assertCount(UserAction.EXIT_INK2_ANNOTATION_MODE, 1);
    chrome.test.succeed();
  },

  // Test that annotation modes can be set when text annotations are enabled.
  async function testSetAnnotationModeTextEnabled() {
    mockMetricsPrivate.reset();

    await enableTextAnnotations(true);

    function assertMetrics(expected: {
      ink2: {enter: number, exit: number},
      text: {enter: number, exit: number},
      sidepanel: number,
    }) {
      mockMetricsPrivate.assertCount(
          UserAction.ENTER_INK2_ANNOTATION_MODE, expected.ink2.enter);
      mockMetricsPrivate.assertCount(
          UserAction.ENTER_INK2_TEXT_ANNOTATION_MODE, expected.text.enter);
      mockMetricsPrivate.assertCount(
          UserAction.EXIT_INK2_ANNOTATION_MODE, expected.ink2.exit);
      mockMetricsPrivate.assertCount(
          UserAction.EXIT_INK2_TEXT_ANNOTATION_MODE, expected.text.exit);

      // Also confirm that the side panel metrics get recorded.
      mockMetricsPrivate.assertCount(
          UserAction.OPEN_INK2_SIDE_PANEL, expected.sidepanel);
      mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 0);
    }

    // Initial state, no metrics recorded.
    assertMetrics(
        {ink2: {enter: 0, exit: 0}, text: {enter: 0, exit: 0}, sidepanel: 0});
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    // NONE -> TEXT
    await setAnnotationMode(AnnotationMode.TEXT);
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);
    assertMetrics(
        {ink2: {enter: 0, exit: 0}, text: {enter: 1, exit: 0}, sidepanel: 1});

    // TEXT -> NONE
    await setAnnotationMode(AnnotationMode.OFF);
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    assertMetrics(
        {ink2: {enter: 0, exit: 0}, text: {enter: 1, exit: 1}, sidepanel: 1});

    // NONE -> DRAW
    await setAnnotationMode(AnnotationMode.DRAW);
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    assertMetrics(
        {ink2: {enter: 1, exit: 0}, text: {enter: 1, exit: 1}, sidepanel: 2});

    // DRAW -> TEXT
    await setAnnotationMode(AnnotationMode.TEXT);
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);
    // Back and forth between draw and text doesn't increment sidepanel entry
    // metric.
    assertMetrics(
        {ink2: {enter: 1, exit: 1}, text: {enter: 2, exit: 1}, sidepanel: 2});

    // TEXT -> DRAW
    await setAnnotationMode(AnnotationMode.DRAW);
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    assertMetrics(
        {ink2: {enter: 2, exit: 1}, text: {enter: 2, exit: 2}, sidepanel: 2});

    // DRAW -> NONE
    await setAnnotationMode(AnnotationMode.OFF);
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    assertMetrics(
        {ink2: {enter: 2, exit: 2}, text: {enter: 2, exit: 2}, sidepanel: 2});

    chrome.test.succeed();
  },

  // Test that the side panel is shown when annotation mode is enabled and
  // hidden when annotation mode is disabled.
  // TODO (crbug.com/402547554): Remove this test once text annotations
  // launches, since it will be fully covered by the test below.
  async function testSidePanelVisible() {
    mockMetricsPrivate.reset();

    // Disable text annotations.
    await enableTextAnnotations(false);

    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    await setAnnotationMode(AnnotationMode.DRAW);
    mockMetricsPrivate.assertCount(UserAction.ENTER_INK2_ANNOTATION_MODE, 1);
    const sidePanel = viewer.shadowRoot.querySelector('viewer-side-panel');
    chrome.test.assertTrue(!!sidePanel);

    // The side panel should be visible when annotation mode is enabled.
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    chrome.test.assertTrue(isVisible(sidePanel));

    await setAnnotationMode(AnnotationMode.OFF);
    mockMetricsPrivate.assertCount(UserAction.EXIT_INK2_ANNOTATION_MODE, 1);

    // The side panel should be hidden when annotation mode is disabled.
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    chrome.test.assertFalse(isVisible(sidePanel));

    chrome.test.succeed();
  },

  // Test that the correct side panel is shown for the annotation mode when
  // text annotations are enabled.
  async function testSidePanelVisibleTextEnabled() {
    mockMetricsPrivate.reset();

    // Enable text annotations.
    await enableTextAnnotations(true);

    const drawPanelQuery = 'viewer-side-panel';
    const textPanelQuery = 'viewer-text-side-panel';

    // Panels are not in the DOM in NONE annotation mode.
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector(textPanelQuery));
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector(drawPanelQuery));

    // In text mode, only the text panel is visible.
    await setAnnotationMode(AnnotationMode.TEXT);
    const drawPanel = viewer.shadowRoot.querySelector(drawPanelQuery);
    const textPanel = viewer.shadowRoot.querySelector(textPanelQuery);
    chrome.test.assertTrue(!!drawPanel);
    chrome.test.assertTrue(!!textPanel);
    chrome.test.assertFalse(isVisible(drawPanel));
    chrome.test.assertTrue(isVisible(textPanel));

    // In draw mode, only the draw panel is visible.
    await setAnnotationMode(AnnotationMode.DRAW);
    chrome.test.assertTrue(isVisible(drawPanel));
    chrome.test.assertFalse(isVisible(textPanel));

    // Both removed from the DOM again when annotation mode is disabled.
    await setAnnotationMode(AnnotationMode.OFF);
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector(textPanelQuery));
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector(drawPanelQuery));

    chrome.test.succeed();
  },

  async function testTextboxVisibility() {
    // Enable text annotations.
    await enableTextAnnotations(true);

    // Annotation mode off. Textbox is not in the DOM.
    await setAnnotationMode(AnnotationMode.OFF);
    let textbox = viewer.shadowRoot.querySelector('ink-text-box');
    chrome.test.assertFalse(!!textbox);

    // Text annotation mode. Textbox is in the DOM but isn't visible.
    await setAnnotationMode(AnnotationMode.TEXT);
    textbox = viewer.shadowRoot.querySelector('ink-text-box');
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertFalse(isVisible(textbox));

    // Simulate clicking the plugin. pdf-viewer should notify Ink2Manager to
    // initialize an annotation, which shows the box.
    PluginController.getInstance().getEventTarget().dispatchEvent(
        new CustomEvent(
            PluginControllerEventType.PLUGIN_MESSAGE,
            {detail: {type: 'sendClickEvent', x: 50, y: 50}}));
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // Switching to a different annotation mode removes the box from the DOM.
    await setAnnotationMode(AnnotationMode.DRAW);
    textbox = viewer.shadowRoot.querySelector('ink-text-box');
    chrome.test.assertFalse(!!textbox);

    // Text annotation mode. Switching back to text puts the box back in the
    // DOM, but does not immediately make it visible.
    await setAnnotationMode(AnnotationMode.TEXT);
    textbox = viewer.shadowRoot.querySelector('ink-text-box');
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertFalse(isVisible(textbox));

    chrome.test.succeed();
  },

  async function testSwitchFromTextToDrawCommits() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Enable text annotations.
    await enableTextAnnotations(true);

    // Switch to TEXT mode.
    await setAnnotationMode(AnnotationMode.TEXT);
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);

    // Create a textbox.
    createTextBox();
    await microtasksFinished();
    const textbox = viewer.shadowRoot.querySelector('ink-text-box')!;
    chrome.test.assertTrue(!!textbox);
    chrome.test.assertTrue(isVisible(textbox));

    // Edit the textbox.
    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Switch to DRAW mode. This should commit the textbox.
    await setAnnotationMode(AnnotationMode.DRAW);

    // Verify textbox is removed and committed.
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector('ink-text-box'));
    const finishMessage = mockPlugin.findMessage('finishTextAnnotation');
    chrome.test.assertNe(undefined, finishMessage);
    chrome.test.succeed();
  },

  async function testSwitchFromTextToOffCommits() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Enable text annotations.
    await enableTextAnnotations(true);

    // Switch to TEXT mode.
    await setAnnotationMode(AnnotationMode.TEXT);

    // Create a textbox.
    createTextBox();
    await microtasksFinished();
    const textbox = viewer.shadowRoot.querySelector('ink-text-box')!;

    // Edit the textbox.
    textbox.$.textbox.value = 'World';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // Switch to OFF mode. This should commit the textbox.
    await setAnnotationMode(AnnotationMode.OFF);

    // Verify textbox is removed and committed.
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector('ink-text-box'));
    const finishMessage = mockPlugin.findMessage('finishTextAnnotation');
    chrome.test.assertNe(undefined, finishMessage);
    chrome.test.succeed();
  },

  async function testPrintCommits() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Enable text annotations and printing.
    loadTimeData.overrideValues({
      'pdfTextAnnotationsEnabled': true,
      'printingEnabled': true,
    });
    viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
    await microtasksFinished();

    // Switch to TEXT mode.
    await setAnnotationMode(AnnotationMode.TEXT);

    // Create a textbox.
    createTextBox();
    await microtasksFinished();
    const textbox = viewer.shadowRoot.querySelector('ink-text-box')!;

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

    // Enable text annotations and printing.
    loadTimeData.overrideValues({
      'pdfTextAnnotationsEnabled': true,
      'printingEnabled': true,
    });
    viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
    await microtasksFinished();

    // Switch to TEXT mode.
    await setAnnotationMode(AnnotationMode.TEXT);

    // Create a textbox.
    createTextBox();
    await microtasksFinished();
    const textbox = viewer.shadowRoot.querySelector('ink-text-box')!;

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
]);
