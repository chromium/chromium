// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function validFileCopyToShouldSucceed() {
    runCopyToTest(validWEBPImageCase, true /* expect success */);
  },
  function invalidFileCopyToShouldFail() {
    runCopyToTest(invalidWEBPImageCase, false /* expect failure */);
  },
]);

