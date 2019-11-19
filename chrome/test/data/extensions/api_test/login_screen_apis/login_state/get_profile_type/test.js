// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetProfileType(expected) {
  chrome.test.runTests([() => {
    chrome.loginState.getProfileType(profileType => {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(expected, profileType);
      chrome.test.succeed();
    });
  }]);
}

chrome.test.getConfig(config => {
  testGetProfileType(config.customArg);
});
