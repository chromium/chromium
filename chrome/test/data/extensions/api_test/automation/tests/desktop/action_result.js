// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let allTests = [function testActionResult() {
  const firstTextField = rootNode.find({role: RoleType.TEXT_FIELD});
  assertTrue(!!firstTextField);
  firstTextField.scrollBackward(result => {
    assertTrue(!!result);
    chrome.test.succeed();
  });
}];

setUpAndRunDesktopTests(allTests)
