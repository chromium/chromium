// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [function testIntents() {
  const text = rootNode.find({role: chrome.automation.RoleType.STATIC_TEXT});
  assertEq('111', text.name);

  rootNode.addEventListener(
      chrome.automation.EventType.TEXT_SELECTION_CHANGED, (e) => {
        assertEq(1, e.intents.length);
        assertEq(
            chrome.automation.EventCommandType.SET_SELECTION,
            e.intents[0].command);
        assertEq(
            chrome.automation.EventTextBoundaryType.CHARACTER,
            e.intents[0].textBoundary);
        assertEq(
            chrome.automation.EventMoveDirectionType.FORWARD,
            e.intents[0].moveDirection);
        chrome.test.succeed();
      }, true);

  text.setSelection(0, 1);
}];

setUpAndRunTests(allTests, 'intents.html');
