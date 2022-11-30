// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function getReferrerChain() {
    chrome.webstorePrivate.getReferrerChain((result) => {
      chrome.test.assertEq("", result);
      chrome.test.succeed();
    });
  },
];

chrome.test.runTests(tests);
