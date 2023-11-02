// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function storageIsDisabled() {
    chrome.storage.managed.get(
        chrome.test.callbackFail(
            '"managed" is not available in this instance of Chrome'));
  }
]);
