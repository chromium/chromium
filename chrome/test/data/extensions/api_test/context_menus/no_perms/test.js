// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function simple() {
    chrome.test.assertEq(undefined, chrome.contextMenus);
    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
