// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([function() {
  const host = document.getElementById('host');
  let shadowRoot = chrome.dom.openOrClosedShadowRoot(host);
  chrome.test.assertEq(shadowRoot.childElementCount, 2);

  let caughtError;
  try {
    chrome.dom.openOrClosedShadowRoot(new Object());
  } catch (error) {
    caughtError = error;
  }

  var expectedError =
      'Error in invocation of dom.openOrClosedShadowRoot(HTMLElement ' +
      'element): '
  chrome.test.assertEq(caughtError.name, 'TypeError');
  chrome.test.assertEq(caughtError.message, expectedError);

  chrome.test.succeed();
}]);
