// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testDoDefault() {
    var firstTextField = findAutomationNode(rootNode,
        function(node) {
          return node.role == 'textField';
        });
    assertTrue(!!firstTextField);
    listenOnce(firstTextField, EventType.FOCUS, function(e) {
      chrome.test.succeed();
    }, true);
    firstTextField.doDefault();
  },

  function testContextMenu() {
    var addressBar = rootNode.find({role: 'textField'});
    listenOnce(rootNode, EventType.MENU_START, function(e) {
      addressBar.showContextMenu();
      chrome.test.succeed();
    }, true);
    addressBar.showContextMenu();
  }
];

setUpAndRunTests(allTests);
