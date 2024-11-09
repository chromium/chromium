// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
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
    chrome.test.assertTrue(viewerToolbar.pdfAnnotationsEnabled);
    chrome.test.succeed();
  },

  // Test that annotation mode can be toggled.
  async function testToggleAnnotationMode() {
    mockMetricsPrivate.reset();

    chrome.test.assertFalse(viewerToolbar.annotationMode);

    viewer.$.toolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    mockMetricsPrivate.assertCount(UserAction.ENTER_INK2_ANNOTATION_MODE, 1);

    viewer.$.toolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    mockMetricsPrivate.assertCount(UserAction.EXIT_INK2_ANNOTATION_MODE, 1);
    chrome.test.succeed();
  },

  // Test that the side panel is shown when annotation mode is enabled and
  // hidden when annotation mode is disabled.
  async function testSidePanelVisible() {
    mockMetricsPrivate.reset();

    chrome.test.assertFalse(viewerToolbar.annotationMode);

    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    mockMetricsPrivate.assertCount(UserAction.ENTER_INK2_ANNOTATION_MODE, 1);
    const sidePanel = viewer.shadowRoot!.querySelector('viewer-side-panel');
    assert(sidePanel);

    // The side panel should be visible when annotation mode is enabled.
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    chrome.test.assertTrue(isVisible(sidePanel));

    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    mockMetricsPrivate.assertCount(UserAction.EXIT_INK2_ANNOTATION_MODE, 1);

    // The side panel should be hidden when annotation mode is disabled.
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    chrome.test.assertFalse(isVisible(sidePanel));

    chrome.test.succeed();
  },
]);
