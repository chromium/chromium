// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupMockMetricsPrivate, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
setupTestMockPluginForInk();
const mockMetricsPrivate = setupMockMetricsPrivate();

chrome.test.runTests([
  // Test that toggling annotation mode opens the bottom toolbar when text
  // and signature annotations are enabled.
  async function testOpenBottomToolbarSignaturesEnabled() {
    mockMetricsPrivate.reset();

    // Enable text and signature annotations.
    loadTimeData.overrideValues({
      'pdfSignaturesEnabled': true,
      'pdfTextAnnotationsEnabled': true,
    });
    viewer.$.toolbar.strings = Object.assign({}, viewer.$.toolbar.strings);
    await microtasksFinished();

    // No toolbars initially.
    const drawToolbarQuery = 'viewer-bottom-toolbar';
    const textToolbarQuery = 'viewer-text-bottom-toolbar';
    const signatureToolbarQuery = 'viewer-signature-bottom-toolbar';
    chrome.test.assertEq(AnnotationMode.OFF, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(drawToolbarQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(textToolbarQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(signatureToolbarQuery));

    // Text annotation mode shows the text toolbar.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.TEXT);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const drawToolbar = viewer.shadowRoot.querySelector(drawToolbarQuery);
    const textToolbar = viewer.shadowRoot.querySelector(textToolbarQuery);
    const signatureToolbar =
        viewer.shadowRoot.querySelector(signatureToolbarQuery);
    chrome.test.assertTrue(isVisible(textToolbar));
    chrome.test.assertFalse(isVisible(drawToolbar));
    chrome.test.assertFalse(isVisible(signatureToolbar));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);

    // Draw annotation mode shows the drawing toolbar.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(isVisible(textToolbar));
    chrome.test.assertTrue(isVisible(drawToolbar));
    chrome.test.assertFalse(isVisible(signatureToolbar));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    // Still 1, because the bottom toolbar is still in use, just in a
    // different mode.
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);

    // Signature annotation mode shows the signature toolbar.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.SIGNATURE);
    await microtasksFinished();
    chrome.test.assertEq(
        AnnotationMode.SIGNATURE, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(isVisible(textToolbar));
    chrome.test.assertFalse(isVisible(drawToolbar));
    chrome.test.assertTrue(isVisible(signatureToolbar));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);

    // No annotation mode removes all toolbars from the DOM.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(drawToolbarQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(textToolbarQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(signatureToolbarQuery));

    chrome.test.succeed();
  },

  // Test that toggling annotation mode opens the bottom toolbar when text
  // annotation is enabled.
  // TODO(crbug.com/410859904): delete this test when signatures is launched
  // because it will be entirely covered by the test above.
  async function testOpenBottomToolbarTextEnabled() {
    mockMetricsPrivate.reset();

    // Enable text annotations.
    loadTimeData.overrideValues({
      'pdfSignaturesEnabled': false,
      'pdfTextAnnotationsEnabled': true,
    });
    viewer.$.toolbar.strings = Object.assign({}, viewer.$.toolbar.strings);
    await microtasksFinished();

    // No toolbars initially.
    const drawToolbarQuery = 'viewer-bottom-toolbar';
    const textToolbarQuery = 'viewer-text-bottom-toolbar';
    chrome.test.assertEq(AnnotationMode.OFF, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(drawToolbarQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(textToolbarQuery));

    // Text annotation mode shows the text toolbar.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.TEXT);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const drawToolbar = viewer.shadowRoot.querySelector(drawToolbarQuery);
    const textToolbar = viewer.shadowRoot.querySelector(textToolbarQuery);
    chrome.test.assertTrue(isVisible(textToolbar));
    chrome.test.assertFalse(isVisible(drawToolbar));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);

    // Draw annotation mode shows the drawing toolbar.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(isVisible(textToolbar));
    chrome.test.assertTrue(isVisible(drawToolbar));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    // Still 1, because the bottom toolbar is still in use, just in a
    // different mode.
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);

    // No annotation mode removes both toolbars from the DOM.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(drawToolbarQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(textToolbarQuery));
    chrome.test.succeed();
  },

  // Test that toggling annotation mode opens the bottom toolbar.
  // TODO(crbug.com/402547554): delete this test after text annotations is
  // launched because it will be fully covered by the test above
  async function testOpenBottomToolbar() {
    mockMetricsPrivate.reset();

    // Disable text annotations and signatures
    loadTimeData.overrideValues({
      'pdfSignaturesEnabled': false,
      'pdfTextAnnotationsEnabled': false,
    });
    viewer.$.toolbar.strings = Object.assign({}, viewer.$.toolbar.strings);
    await microtasksFinished();

    viewer.$.toolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();

    chrome.test.assertEq(AnnotationMode.DRAW, viewer.$.toolbar.annotationMode);
    chrome.test.assertTrue(
        !!viewer.shadowRoot.querySelector('viewer-bottom-toolbar'));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);
    chrome.test.succeed();
  },
]);
