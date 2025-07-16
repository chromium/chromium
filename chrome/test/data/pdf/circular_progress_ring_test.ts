// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {microtasksFinished} from 'chrome://webui-test/test_util.js';

const tests = [
  /**
   * Test the circular progress ring's stroke dash offset values
   */
  async function testValueUpdate() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element = document.createElement('circular-progress-ring');
    document.body.appendChild(element);

    element.value = 0;
    await microtasksFinished();
    chrome.test.assertEq(
        '566px', element.$.innerProgress.getAttribute('stroke-dashoffset'));

    element.value = 30;
    await microtasksFinished();
    chrome.test.assertEq(
        '396px', element.$.innerProgress.getAttribute('stroke-dashoffset'));

    element.value = 83;
    await microtasksFinished();
    chrome.test.assertEq(
        '96px', element.$.innerProgress.getAttribute('stroke-dashoffset'));

    element.value = 100;
    await microtasksFinished();
    chrome.test.assertEq(
        '0px', element.$.innerProgress.getAttribute('stroke-dashoffset'));

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
