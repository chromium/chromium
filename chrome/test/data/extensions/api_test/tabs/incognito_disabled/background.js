// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function createIncognitoWindow() {
    chrome.windows.create({ url: "about:blank", incognito: true },
                          function(window) {
      chrome.test.assertEq(window, undefined);
      chrome.test.notifyPass();
    });
  }
]);
