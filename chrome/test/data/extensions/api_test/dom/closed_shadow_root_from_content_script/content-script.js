// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function() {
  const host = document.getElementById('host');
  let shadowRoot = chrome.dom.openOrClosedShadowRoot(host);
  chrome.test.assertEq(shadowRoot.childElementCount, 2);
  chrome.test.assertThrows(
      chrome.dom.openOrClosedShadowRoot, [new Object()],
      'Error in invocation of dom.openOrClosedShadowRoot(HTMLElement ' +
          'element): ');
  chrome.test.succeed();
}]);
