// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertContains(string, substring, error) {
  chrome.test.assertNe(-1, string.indexOf(substring), error);
}

chrome.test.runTests([
  function testOpenDatabase() {
    chrome.test.assertTrue(!window.openDatabase);
    chrome.test.succeed();
  },

  function testOpenDatabaseSync() {
    chrome.test.assertTrue(!window.openDatabaseSync);
    chrome.test.succeed();
  },

  function testLocalStorage() {
    chrome.test.assertTrue(!window.localStorage);
    chrome.test.succeed();
  }
]);
