// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testGetTargetsThrows() {
    chrome.test.assertEq(undefined, chrome.debugger);
    chrome.test.succeed();
  }
]);
