// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';

import {getCurrentPage, getRequiredElement} from './test_util.js';

function simulateRendererPreferencesUpdated(caretBrowsingEnabled: boolean) {
  const viewer = document.body.querySelector<HTMLElement>('pdf-viewer')!;
  const plugin = getRequiredElement(viewer, 'embed');
  plugin.dispatchEvent(new MessageEvent(
      'message',
      {data: {type: 'rendererPreferencesUpdated', caretBrowsingEnabled}}));
}

chrome.test.runTests([
  function testArrowLeftRight() {
    chrome.test.assertEq(0, getCurrentPage());

    simulateRendererPreferencesUpdated(true);
    pressAndReleaseKeyOn(document.documentElement, 0, [], 'ArrowRight');
    chrome.test.assertEq(0, getCurrentPage());

    simulateRendererPreferencesUpdated(false);
    pressAndReleaseKeyOn(document.documentElement, 0, [], 'ArrowRight');
    chrome.test.assertEq(1, getCurrentPage());

    simulateRendererPreferencesUpdated(true);
    pressAndReleaseKeyOn(document.documentElement, 0, [], 'ArrowLeft');
    chrome.test.assertEq(1, getCurrentPage());

    simulateRendererPreferencesUpdated(false);
    pressAndReleaseKeyOn(document.documentElement, 0, [], 'ArrowLeft');
    chrome.test.assertEq(0, getCurrentPage());
    chrome.test.succeed();
  },
]);
