// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;

chrome.test.runTests([
  // Test that the annotations bar is shown when annotation mode is enabled and
  // hidden when annotation mode is disabled.
  async function testAnnotationBar() {
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    viewerToolbar.toggleAnnotation();
    await waitAfterNextRender(viewerToolbar);
    const annotationsBar =
        viewerToolbar.shadowRoot!.querySelector('viewer-annotations-bar');

    // Annotations bar should be visible when annotation mode is enabled.
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    chrome.test.assertTrue(isVisible(annotationsBar));

    viewerToolbar.toggleAnnotation();
    await waitAfterNextRender(viewerToolbar);

    // Annotations bar should be hidden when annotation mode is disabled.
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    chrome.test.assertFalse(isVisible(annotationsBar));
    chrome.test.succeed();
  },
]);
