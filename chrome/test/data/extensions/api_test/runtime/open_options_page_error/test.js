// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// browser_tests --gtest_filter=ExtensionApiTest.OpenOptionsPageError

function test() {
  chrome.runtime.openOptionsPage(
      chrome.test.callbackFail('Could not create an options page.'));
}

chrome.test.runTests([test]);
