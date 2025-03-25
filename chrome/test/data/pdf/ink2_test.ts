// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupMockMetricsPrivate, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
setupTestMockPluginForInk();
const mockMetricsPrivate = setupMockMetricsPrivate();

chrome.test.runTests([
  // Test that PDF annotations and the new ink mode are enabled.
  function testAnnotationsEnabled() {
    chrome.test.assertTrue(loadTimeData.getBoolean('pdfAnnotationsEnabled'));
    chrome.test.assertTrue(loadTimeData.getBoolean('pdfInk2Enabled'));
    // When ink2 and annotations are enabled in loadTimeData, the ink2
    // button section displays.
    chrome.test.assertTrue(
        !!viewerToolbar.shadowRoot.querySelector('#annotate-controls'));
    chrome.test.succeed();
  },

  // Test that annotation mode can be set.
  async function testSetAnnotationMode() {
    mockMetricsPrivate.reset();

    chrome.test.assertEq(AnnotationMode.NONE, viewerToolbar.annotationMode);

    viewer.$.toolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    mockMetricsPrivate.assertCount(UserAction.ENTER_INK2_ANNOTATION_MODE, 1);

    viewer.$.toolbar.setAnnotationMode(AnnotationMode.NONE);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.NONE, viewerToolbar.annotationMode);
    mockMetricsPrivate.assertCount(UserAction.EXIT_INK2_ANNOTATION_MODE, 1);
    chrome.test.succeed();
  },

  // Test that the side panel is shown when annotation mode is enabled and
  // hidden when annotation mode is disabled.
  async function testSidePanelVisible() {
    mockMetricsPrivate.reset();

    chrome.test.assertEq(AnnotationMode.NONE, viewerToolbar.annotationMode);

    viewerToolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    mockMetricsPrivate.assertCount(UserAction.ENTER_INK2_ANNOTATION_MODE, 1);
    const sidePanel = viewer.shadowRoot.querySelector('viewer-side-panel');
    assert(sidePanel);

    // The side panel should be visible when annotation mode is enabled.
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    chrome.test.assertTrue(isVisible(sidePanel));

    viewerToolbar.setAnnotationMode(AnnotationMode.NONE);
    await microtasksFinished();
    mockMetricsPrivate.assertCount(UserAction.EXIT_INK2_ANNOTATION_MODE, 1);

    // The side panel should be hidden when annotation mode is disabled.
    chrome.test.assertEq(AnnotationMode.NONE, viewerToolbar.annotationMode);
    chrome.test.assertFalse(isVisible(sidePanel));

    chrome.test.succeed();
  },
]);
