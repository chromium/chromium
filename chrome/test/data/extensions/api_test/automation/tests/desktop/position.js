// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testPositionType() {
    var button = rootNode.find({role: chrome.automation.RoleType.BUTTON});
    var position = button.createPosition(
        /* type */ chrome.automation.PositionType.TREE, /* offset */ 0);

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
    var pos1 = childOfRoot.createPosition(
        /* type */ chrome.automation.PositionType.TREE, /* offset */ 0);
    var pos2 = rootNode.createPosition(
        /* type */ chrome.automation.PositionType.TREE, /* offset */ 0);
    assertFalse(pos1.node == pos2.node, 'Nodes expected to differ');
    chrome.test.succeed();
  },

  function testCrossRoots() {
    chrome.automation.getDesktop(() => {
      var rootPosition = rootNode.createPosition(
          /* type */ chrome.automation.PositionType.TREE, /* offset */ 0);
      rootPosition.moveToParentPosition();
      assertFalse(rootPosition.isNullPosition());
      assertTrue(!!rootPosition.node);
      chrome.test.succeed();
    });
  },

  function testBeginningOfContentEditable() {
    var editable =
        rootNode.find({role: chrome.automation.RoleType.GENERIC_CONTAINER});
    assertTrue(Boolean(editable));
    var position = editable.createPosition(
        /* type */ chrome.automation.PositionType.TEXT, /*offset=*/ 0);
    assertTrue(position.isTextPosition());

    position.asLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq('This', position.node.name);
    assertEq(0, position.textOffset);
    position.moveToNextLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq(' ', position.node.name);
    assertEq(0, position.textOffset);
    position.moveToNextLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq('is', position.node.name);
    assertEq(0, position.textOffset);
    position.moveToNextLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq(' a', position.node.name);
    assertEq(0, position.textOffset);
    position.moveToNextLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq(' test', position.node.name);
    assertEq(0, position.textOffset);

    chrome.test.succeed();
  },

  function testMiddleOfContentEditable() {
    var editable =
        rootNode.find({role: chrome.automation.RoleType.GENERIC_CONTAINER});
    assertTrue(Boolean(editable));
    // The editable value is "This is a test" (all words are contaiend in
    // separate nodes), so an index of 10 points to the beginning of "test".
    var position = editable.createPosition(
        /* type */ chrome.automation.PositionType.TEXT, /*offset=*/ 10);
    assertTrue(position.isTextPosition());
    position.asLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq(' test', position.node.name);
    assertEq(1, position.textOffset);

    position = editable.createPosition(
        /* type */ chrome.automation.PositionType.TEXT, /*offset=*/ 9);
    assertTrue(position.isTextPosition());
    position.asLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq(' a', position.node.name);
    assertEq(2, position.textOffset);

    // An index of 12 points to the middle of "test".
    position = editable.createPosition(
        /* type */ chrome.automation.PositionType.TEXT, /*offset=*/ 12);
    assertTrue(position.isTextPosition());
    position.asLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq(' test', position.node.name);
    assertEq(3, position.textOffset);

    chrome.test.succeed();
  },

  function testIsTextPositionEdgeCases() {
    var editable =
        rootNode.find({role: chrome.automation.RoleType.GENERIC_CONTAINER});
    assertTrue(Boolean(editable));

    // Invalid indices should be corrected and set at either the beginning or
    // the end of the editable value.
    var position = editable.createPosition(
        /* type */ chrome.automation.PositionType.TEXT, /*offset=*/ -1);
    assertTrue(position.isTextPosition());
    position.asLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq('This', position.node.name);
    assertEq(0, position.textOffset);

    // The value length is 14, so test that a correction is made when creating
    // the position.
    position = editable.createPosition(
        /* type */ chrome.automation.PositionType.TEXT, /*offset=*/ 14);
    assertTrue(position.isTextPosition());
    position.asLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq(' test', position.node.name);
    assertEq(5, position.textOffset);

    position = editable.createPosition(
        /* type */ chrome.automation.PositionType.TEXT, /*offset=*/ 100);
    assertTrue(position.isTextPosition());
    position.asLeafTextPosition();
    assertTrue(position.isTextPosition());
    assertEq(' test', position.node.name);
    assertEq(5, position.textOffset);

    chrome.test.succeed();
  }
];

setUpAndRunTabsTests(allTests, 'position.html');
