// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testPositionType() {
    var button = rootNode.find({role: chrome.automation.RoleType.BUTTON});
    var position = button.createPosition(0 /* offset */);

    assertEq(button, position.node);

    assertFalse(position.isNullPosition());
    assertTrue(position.isTreePosition());
    assertFalse(position.isTextPosition());
    assertFalse(position.isLeafTextPosition());

    assertEq(0, position.childIndex);
    assertEq(-1, position.textOffset);
    assertEq('downstream', position.affinity);
    chrome.test.succeed();
  },

  function testBackingObjectsDiffer() {
    var childOfRoot = rootNode.lastChild;
    var pos1 = childOfRoot.createPosition(-1);
    var pos2 = rootNode.createPosition(-1);
    assertFalse(pos1.node == pos2.node, 'Nodes expected to differ');
    chrome.test.succeed();
  },

  function testCrossRoots() {
    chrome.automation.getDesktop(() => {
      var rootPosition = rootNode.createPosition(-1);
      rootPosition.moveToParentPosition();
      assertTrue(!!rootPosition.node);
      chrome.test.succeed();
    });
  }
];

setUpAndRunTestsInPage(allTests, 'position.html');
