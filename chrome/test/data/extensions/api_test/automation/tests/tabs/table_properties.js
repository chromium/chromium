// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: `RoleType` is defined in automation/tests/desktop/common.js.

const allTests = [
  function tableProperties() {
    const cell = rootNode.find({role: RoleType.CELL});
    assertEq('Cell 1', cell.name);
    assertEq('Header 1', cell.tableCellColumnHeaders[0].name);
    const cell2 = cell.nextSibling;
    assertEq('Cell 2', cell2.name);
    assertEq('Header 2', cell2.tableCellColumnHeaders[0].name);
    chrome.test.succeed();
  },
];

setUpAndRunTabsTests(allTests, 'table.html');
