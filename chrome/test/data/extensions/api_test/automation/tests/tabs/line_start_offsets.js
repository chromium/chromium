// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const allTests = [
  function testInput() {
    const textFields = rootNode.findAll({role: RoleType.TEXT_FIELD});
    assertEq(2, textFields.length);
    const input = textFields[0];
    assertTrue(!!input);
    assertTrue('lineStartOffsets' in input);
    const lineStarts = input.lineStartOffsets;
    assertEq(1, lineStarts.length);
    assertEq(0, lineStarts[0]);
    chrome.test.succeed();
  },

  function testTextarea() {
    const textFields = rootNode.findAll({role: RoleType.TEXT_FIELD});
    assertEq(2, textFields.length);
    const textarea = textFields[1];
    assertTrue(!!textarea);
    assertTrue('lineStartOffsets' in textarea);
    const lineStarts = textarea.lineStartOffsets;
    assertEq(3, lineStarts.length);
    assertEq(0, lineStarts[0]);
    assertEq(10, lineStarts[1]);
    assertEq(20, lineStarts[2]);
    chrome.test.succeed();
  },
];

setUpAndRunTabsTests(allTests, 'line_start_offsets.html');
