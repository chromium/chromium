// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

const viewer = document.body.querySelector('pdf-viewer')!;

chrome.test.runTests([
  // Test that the annotate controls aren't shown when annotation mode is
  // disabled.
  function testAnnotationsDisabled() {
    chrome.test.assertFalse(loadTimeData.getBoolean('pdfInk2Enabled'));

    const viewerToolbar = viewer.$.toolbar;
    // When ink2 and annotations are disabled in loadTimeData, the ink2
    // button section is not in the DOM.
    chrome.test.assertFalse(
        !!viewerToolbar.shadowRoot.querySelector('#annotate-controls'));
    chrome.test.succeed();
  },
]);
