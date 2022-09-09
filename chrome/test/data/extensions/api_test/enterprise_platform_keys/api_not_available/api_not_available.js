// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function notAvailable() {
    chrome.test.assertTrue(!chrome.enterprise ||
                           !chrome.enterprise.platformKeys);
    chrome.test.succeed();
  }
]);
