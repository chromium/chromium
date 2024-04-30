// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

const viewer = document.body.querySelector('pdf-viewer')!;

chrome.test.runTests([
  // Test that PDF annotations and the new ink mode are enabled.
  function testAnnotationsEnabled() {
    chrome.test.assertTrue(loadTimeData.getBoolean('pdfAnnotationsEnabled'));
    chrome.test.assertTrue(loadTimeData.getBoolean('pdfInk2Enabled'));
    chrome.test.assertTrue(viewer.$.toolbar.pdfAnnotationsEnabled);
    chrome.test.succeed();
  },

  // Test that annotation mode can be toggled.
  function testToggleAnnotationMode() {
    chrome.test.assertFalse(viewer.$.toolbar.annotationMode);

    viewer.$.toolbar.toggleAnnotation();
    chrome.test.assertTrue(viewer.$.toolbar.annotationMode);

    viewer.$.toolbar.toggleAnnotation();
    chrome.test.assertFalse(viewer.$.toolbar.annotationMode);
    chrome.test.succeed();
  },
]);
