// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

chrome.test.runTests([
  async function testPrintingEnabled() {
    loadTimeData.overrideValues({'printingEnabled': true});
    const viewer = document.body.querySelector('pdf-viewer')!;
    const toolbar = viewer.shadowRoot.querySelector('viewer-toolbar')!;
    // Reset strings so toolbar picks up the new loadTimeData values.
    toolbar.strings = Object.assign({}, toolbar.strings);
    await microtasksFinished();
    const printIcon = toolbar.shadowRoot.querySelector<HTMLElement>('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertFalse(printIcon.hidden);
    chrome.test.succeed();
  },
  async function testPrintingDisabled() {
    loadTimeData.overrideValues({'printingEnabled': false});
    const viewer = document.body.querySelector('pdf-viewer')!;
    const toolbar = viewer.shadowRoot.querySelector('viewer-toolbar')!;
    // Reset strings so toolbar picks up the new loadTimeData values.
    toolbar.strings = Object.assign({}, toolbar.strings);
    await microtasksFinished();
    const printIcon = toolbar.shadowRoot.querySelector<HTMLElement>('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertTrue(printIcon.hidden);
    chrome.test.succeed();
  },
]);
