// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetSessionState(expected) {
  chrome.test.runTests([() => {
    chrome.loginState.getSessionState(sessionState => {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(expected, sessionState);
      chrome.test.succeed();
    });
  }]);
}

chrome.test.getConfig(config => {
  testGetSessionState(config.customArg);
});
