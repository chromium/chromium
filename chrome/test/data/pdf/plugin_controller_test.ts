// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const tests = [
  async function testRequestThumbnail() {
    const data = await PluginController.getInstance().requestThumbnail(0);

    const expectedWidth = Math.floor(108 * window.devicePixelRatio);
    const expectedHeight = Math.floor(140 * window.devicePixelRatio);
    chrome.test.assertEq(expectedWidth, data.width);
    chrome.test.assertEq(expectedHeight, data.height);

    const expectedByteLength = expectedWidth * expectedHeight * 4;
    chrome.test.assertEq(expectedByteLength, data.imageData.byteLength);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
