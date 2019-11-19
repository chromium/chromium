// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testIgnoredNodesNotReturned() {
    var node = rootNode.find({role: chrome.automation.RoleType.STATIC_TEXT});
    assertEq('This is a test', node.name);
    assertEq(1, node.children.length);
    assertEq(chrome.automation.RoleType.INLINE_TEXT_BOX, node.firstChild.role);
    assertEq('This is a test', node.firstChild.name);

    // The line break is ignored and should not show up.
    assertEq(undefined, node.nextOnLine);
    assertEq(undefined, node.firstChild.nextOnLine);

    node = node.nextSibling;
    assertEq('of a content editable.', node.name);
    assertEq(1, node.children.length);
    assertEq(chrome.automation.RoleType.INLINE_TEXT_BOX, node.firstChild.role);
    assertEq('of a content editable.', node.firstChild.name);
    chrome.test.succeed();
  }
];

setUpAndRunTests(allTests, 'ignored_nodes_not_returned.html');
