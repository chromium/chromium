// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testHitTest() {
    var buttons = rootNode.findAll({ role: RoleType.BUTTON });
    var button1 = buttons[0];
    assertEq(button1.name, 'Hit Test 1');
    var x = button1.location.left + button1.location.width / 2;
    var y = button1.location.top + button1.location.height / 2;
    var button2 = buttons[1];
    assertEq(button2.name, 'Hit Test 2');
    var webArea = button1.parent;
    while (webArea.role != RoleType.ROOT_WEB_AREA)
      webArea = webArea.parent;
    button1.addEventListener(EventType.HOVER, function() {
      x = button2.location.left + button2.location.width / 2;
      y = button2.location.top + button2.location.height / 2;
      button2.addEventListener(EventType.CLICKED, function() {
        chrome.test.succeed();
      }, true);
      webArea.hitTest(x, y, EventType.CLICKED);
    }, true);
    webArea.hitTest(x, y, EventType.HOVER);
  }
];

setUpAndRunTabsTests(allTests, 'hit_test.html');
