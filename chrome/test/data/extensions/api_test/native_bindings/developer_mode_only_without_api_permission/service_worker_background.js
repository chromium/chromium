// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testApiIsUndefined() {
    chrome.test.assertEq(undefined, chrome.userScripts);
    chrome.test.succeed();
  }
]);
