// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function enumerateTabs() {
    chrome.windows.getAll({"populate": true}, function (windows) {
      chrome.test.succeed();
    });
  },
  function getAllCookieStores() {
    chrome.cookies.getAllCookieStores(function (stores) {
      chrome.test.succeed();
    });
  }
]);
