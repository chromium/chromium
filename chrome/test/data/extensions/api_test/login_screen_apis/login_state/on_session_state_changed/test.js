// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testOnSessionStateChanged() {
    chrome.loginState.onSessionStateChanged.addListener(sessionState => {
      chrome.test.assertNoLastError();
      chrome.test.sendMessage(sessionState);
    });
    chrome.test.succeed();
  },
]);
