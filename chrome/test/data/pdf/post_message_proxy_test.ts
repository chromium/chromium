// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PdfViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;

const tests = [
  async function testNoToken() {
    const whenConnectionDenied =
        eventToPromise('connection-denied-for-testing', viewer);
    window.postMessage({type: 'connect'});
    await whenConnectionDenied;
    chrome.test.succeed();
  },
  async function testBadToken() {
    const whenConnectionDenied =
        eventToPromise('connection-denied-for-testing', viewer);
    window.postMessage({type: 'connect', token: 'foo'});
    await whenConnectionDenied;
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
