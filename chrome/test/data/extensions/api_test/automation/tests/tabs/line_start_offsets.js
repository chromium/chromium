// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testInput() {
    var textFields = rootNode.findAll({ role: RoleType.TEXT_FIELD });
    assertEq(2, textFields.length);
    var input = textFields[0];
    assertTrue(!!input);
    assertTrue('lineStartOffsets' in input);
    var lineStarts = input.lineStartOffsets;
    assertEq(1, lineStarts.length);
    assertEq(0, lineStarts[0]);
    chrome.test.succeed();
  },

  function testTextarea() {
    var textFields = rootNode.findAll({ role: RoleType.TEXT_FIELD });
    assertEq(2, textFields.length);
    var textarea = textFields[1];
    assertTrue(!!textarea);
    assertTrue('lineStartOffsets' in textarea);
    var lineStarts = textarea.lineStartOffsets;
    assertEq(3, lineStarts.length);
    assertEq(0, lineStarts[0]);
    assertEq(10, lineStarts[1]);
    assertEq(20, lineStarts[2]);
    chrome.test.succeed();
  }
];

setUpAndRunTabsTests(allTests, 'line_start_offsets.html');
