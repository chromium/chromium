// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function readCookies() {
    chrome.test.assertEq(undefined, chrome.cookies);
    chrome.test.succeed();
  }
]);
