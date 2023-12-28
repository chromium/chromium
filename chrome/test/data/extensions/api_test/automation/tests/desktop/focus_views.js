// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testFocusLocationBar() {
    var firstFocusableNode = findAutomationNode(rootNode,
        function(node) {
          return node.role == 'textField' && node.state.focusable;
        });

    assertTrue(!!firstFocusableNode);
    listenOnce(firstFocusableNode, EventType.FOCUS, function(e) {
      chrome.test.succeed();
    }, true);
    firstFocusableNode.focus();
  }
];

setUpAndRunDesktopTests(allTests);
