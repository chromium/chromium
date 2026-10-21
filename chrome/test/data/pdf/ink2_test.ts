// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupMockMetricsPrivate, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
setupTestMockPluginForInk();
const mockMetricsPrivate = setupMockMetricsPrivate();

async function enableTextAnnotations(enabled: boolean) {
  loadTimeData.overrideValues({'pdfTextAnnotationsEnabled': enabled});
  viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
  await microtasksFinished();
}

async function enableSignatures(enabled: boolean) {
  loadTimeData.overrideValues({'pdfSignaturesEnabled': enabled});
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

    // Disable text annotations and signatures.
    await enableTextAnnotations(false);
    await enableSignatures(false);
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
  // TODO(crbug.com/410859904): Remove this test once signatures launches,
  // since it will be fully covered by the test below.
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

  // Test that annotation modes can be set when signature mode is enabled.
  async function testSetAnnotationModeSignaturesEnabled() {
    mockMetricsPrivate.reset();

    await enableTextAnnotations(true);
    await enableSignatures(true);

    function assertMetrics(expected: {
      ink2: {enter: number, exit: number},
      text: {enter: number, exit: number},
      signatures: {enter: number, exit: number},
      sidepanel: number,
    }) {
      mockMetricsPrivate.assertCount(
          UserAction.ENTER_INK2_ANNOTATION_MODE, expected.ink2.enter);
      mockMetricsPrivate.assertCount(
          UserAction.ENTER_INK2_TEXT_ANNOTATION_MODE, expected.text.enter);
      mockMetricsPrivate.assertCount(
          UserAction.ENTER_INK2_SIGNATURE_MODE, expected.signatures.enter);
      mockMetricsPrivate.assertCount(
          UserAction.EXIT_INK2_ANNOTATION_MODE, expected.ink2.exit);
      mockMetricsPrivate.assertCount(
          UserAction.EXIT_INK2_TEXT_ANNOTATION_MODE, expected.text.exit);
      mockMetricsPrivate.assertCount(
          UserAction.EXIT_INK2_SIGNATURE_MODE, expected.signatures.exit);

      // Also confirm that the side panel metrics get recorded.
      mockMetricsPrivate.assertCount(
          UserAction.OPEN_INK2_SIDE_PANEL, expected.sidepanel);
      mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 0);
    }

    // Initial state, no metrics recorded.
    assertMetrics({
      ink2: {enter: 0, exit: 0},
      text: {enter: 0, exit: 0},
      signatures: {enter: 0, exit: 0},
      sidepanel: 0,
    });
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    // NONE -> TEXT
    await setAnnotationMode(AnnotationMode.TEXT);
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 0, exit: 0},
      text: {enter: 1, exit: 0},
      signatures: {enter: 0, exit: 0},
      sidepanel: 1,
    });

    // TEXT -> NONE
    await setAnnotationMode(AnnotationMode.OFF);
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 0, exit: 0},
      text: {enter: 1, exit: 1},
      signatures: {enter: 0, exit: 0},
      sidepanel: 1,
    });

    // NONE -> DRAW
    await setAnnotationMode(AnnotationMode.DRAW);
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 1, exit: 0},
      text: {enter: 1, exit: 1},
      signatures: {enter: 0, exit: 0},
      sidepanel: 2,
    });

    // DRAW -> TEXT
    await setAnnotationMode(AnnotationMode.TEXT);
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);
    // Back and forth between draw and text doesn't increment sidepanel entry
    // metric.
    assertMetrics({
      ink2: {enter: 1, exit: 1},
      text: {enter: 2, exit: 1},
      signatures: {enter: 0, exit: 0},
      sidepanel: 2,
    });

    // TEXT -> DRAW
    await setAnnotationMode(AnnotationMode.DRAW);
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 2, exit: 1},
      text: {enter: 2, exit: 2},
      signatures: {enter: 0, exit: 0},
      sidepanel: 2,
    });

    // DRAW -> NONE
    await setAnnotationMode(AnnotationMode.OFF);
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 2, exit: 2},
      text: {enter: 2, exit: 2},
      signatures: {enter: 0, exit: 0},
      sidepanel: 2,
    });

    // NONE -> SIGNATURE
    await setAnnotationMode(AnnotationMode.SIGNATURE);
    chrome.test.assertEq(
        AnnotationMode.SIGNATURE, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 2, exit: 2},
      text: {enter: 2, exit: 2},
      signatures: {enter: 1, exit: 0},
      sidepanel: 3,
    });

    // SIGNATURE -> NONE
    await setAnnotationMode(AnnotationMode.OFF);
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 2, exit: 2},
      text: {enter: 2, exit: 2},
      signatures: {enter: 1, exit: 1},
      sidepanel: 3,
    });

    // NONE -> SIGNATURE
    await setAnnotationMode(AnnotationMode.SIGNATURE);
    chrome.test.assertEq(
        AnnotationMode.SIGNATURE, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 2, exit: 2},
      text: {enter: 2, exit: 2},
      signatures: {enter: 2, exit: 1},
      sidepanel: 4,
    });

    // SIGNATURE -> TEXT
    await setAnnotationMode(AnnotationMode.TEXT);
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 2, exit: 2},
      text: {enter: 3, exit: 2},
      signatures: {enter: 2, exit: 2},
      sidepanel: 4,
    });

    // TEXT -> NONE
    await setAnnotationMode(AnnotationMode.OFF);
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    assertMetrics({
      ink2: {enter: 2, exit: 2},
      text: {enter: 3, exit: 3},
      signatures: {enter: 2, exit: 2},
      sidepanel: 4,
    });

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
    await enableSignatures(false);

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
  // TODO(crbug.com/410859904): Remove this test once signatures
  // launches, since it will be fully covered by the test below.
  async function testSidePanelVisibleTextEnabled() {
    mockMetricsPrivate.reset();

    // Enable text annotations.
    await enableTextAnnotations(true);
    await enableSignatures(false);

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

  // Test that the correct side panel is shown for the annotation mode when
  // text and signature annotations are enabled.
  async function testSidePanelVisibleSignaturesEnabled() {
    mockMetricsPrivate.reset();

    // Enable text and signature annotations.
    await enableTextAnnotations(true);
    await enableSignatures(true);

    const drawPanelQuery = 'viewer-side-panel';
    const textPanelQuery = 'viewer-text-side-panel';
    const signaturePanelQuery = 'viewer-signature-side-panel';

    // Panels are not in the DOM in NONE annotation mode.
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector(textPanelQuery));
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector(drawPanelQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(signaturePanelQuery));

    // In text mode, only the text panel is visible.
    await setAnnotationMode(AnnotationMode.TEXT);
    const drawPanel = viewer.shadowRoot.querySelector(drawPanelQuery);
    const textPanel = viewer.shadowRoot.querySelector(textPanelQuery);
    const signaturePanel = viewer.shadowRoot.querySelector(signaturePanelQuery);
    chrome.test.assertTrue(!!drawPanel);
    chrome.test.assertTrue(!!textPanel);
    chrome.test.assertTrue(!!signaturePanel);
    chrome.test.assertFalse(isVisible(drawPanel));
    chrome.test.assertTrue(isVisible(textPanel));
    chrome.test.assertFalse(isVisible(signaturePanel));

    // In draw mode, only the draw panel is visible.
    await setAnnotationMode(AnnotationMode.DRAW);
    chrome.test.assertTrue(isVisible(drawPanel));
    chrome.test.assertFalse(isVisible(textPanel));
    chrome.test.assertFalse(isVisible(signaturePanel));

    // In signature mode, only the signature panel is visible.
    await setAnnotationMode(AnnotationMode.SIGNATURE);
    chrome.test.assertFalse(isVisible(drawPanel));
    chrome.test.assertFalse(isVisible(textPanel));
    chrome.test.assertTrue(isVisible(signaturePanel));

    // All removed from the DOM again when annotation mode is disabled.
    await setAnnotationMode(AnnotationMode.OFF);
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector(textPanelQuery));
    chrome.test.assertFalse(!!viewer.shadowRoot.querySelector(drawPanelQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(signaturePanelQuery));

    chrome.test.succeed();
  },
]);
