// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

const viewer = document.body.querySelector('pdf-viewer')!;

chrome.test.runTests([
  // Test that the annotate button isn't shown when annotation mode is disabled.
  function testAnnotationsDisabled() {
    chrome.test.assertFalse(loadTimeData.getBoolean('pdfAnnotationsEnabled'));
    chrome.test.assertFalse(loadTimeData.getBoolean('pdfInk2Enabled'));

    const viewerToolbar = viewer.$.toolbar;
    chrome.test.assertFalse(viewerToolbar.pdfAnnotationsEnabled);

    const annotateButton = viewerToolbar.shadowRoot!.querySelector('#annotate');
    chrome.test.assertTrue(annotateButton === null);
    chrome.test.succeed();
  },
]);
