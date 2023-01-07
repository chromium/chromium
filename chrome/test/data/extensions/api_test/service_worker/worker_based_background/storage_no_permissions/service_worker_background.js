// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testStorageUndefined() {
    try {
      chrome.test.assertEq('undefined', typeof(chrome.storage));
      chrome.test.succeed();
    }
    catch (e) {
      chrome.test.fail(e);
    }
  },
]);
