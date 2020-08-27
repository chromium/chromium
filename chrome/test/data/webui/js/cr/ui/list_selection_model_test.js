// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createSelectionModel(len, opt_dependentLeadItem) {
  var sm = new cr.ui.ListSelectionModel(len);
  sm.independentLeadItem_ = !opt_dependentLeadItem;
  return sm;
}

function testAdjust1() {
  var sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 100;
  adjust(sm, 0, 10, 0);

  assertEquals(90, sm.leadIndex);
  assertEquals(90, sm.anchorIndex);
  assertEquals(90, sm.selectedIndex);
}

function testAdjust2() {
  var sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 50;
  adjust(sm, 60, 10, 0);

  assertEquals(50, sm.leadIndex);
  assertEquals(50, sm.anchorIndex);
  assertEquals(50, sm.selectedIndex);
}

function testAdjust3() {
  var sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 100;
  adjust(sm, 0, 0, 10);

  assertEquals(110, sm.leadIndex);
  assertEquals(110, sm.anchorIndex);
  assertEquals(110, sm.selectedIndex);
}

function testAdjust4() {
  var sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = 100;
  sm.selectRange(100, 110);

  adjust(sm, 0, 10, 5);

  assertEquals(95, sm.leadIndex);
  assertEquals(95, sm.anchorIndex);
  assertArrayEquals(range(95, 105), sm.selectedIndexes);
}

function testAdjust5() {
  var sm = createSelectionModel(100);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 99;

  adjust(sm, 99, 1, 0);

  assertEquals(98, sm.leadIndex, 'lead');
  assertEquals(98, sm.anchorIndex, 'anchor');
  assertArrayEquals([98], sm.selectedIndexes);
}

function testAdjust6() {
  var sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = 105;
  sm.selectRange(100, 110);

  // Remove 100 - 105
  adjust(sm, 100, 5, 0);

  assertEquals(100, sm.leadIndex, 'lead');
  assertEquals(100, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(100, 105), sm.selectedIndexes);
}

function testAdjust7() {
  var sm = createSelectionModel(1);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 0;

  adjust(sm, 0, 0, 10);

  assertEquals(10, sm.leadIndex, 'lead');
  assertEquals(10, sm.anchorIndex, 'anchor');
  assertArrayEquals([10], sm.selectedIndexes);
}

function testAdjust8() {
  var sm = createSelectionModel(100);

  sm.leadIndex = sm.anchorIndex = 50;
  sm.selectAll();

  adjust(sm, 10, 80, 0);

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(0, 19), sm.selectedIndexes);
}

function testAdjust9() {
  var sm = createSelectionModel(10);

  sm.leadIndex = sm.anchorIndex = 5;
  sm.selectAll();

  // Remove all
  adjust(sm, 0, 10, 0);

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals([], sm.selectedIndexes);
}

function testAdjust10() {
  var sm = createSelectionModel(10);

  sm.leadIndex = sm.anchorIndex = 5;
  sm.selectAll();

  adjust(sm, 0, 10, 20);

  assertEquals(5, sm.leadIndex, 'lead');
  assertEquals(5, sm.anchorIndex, 'anchor');
  assertArrayEquals([5], sm.selectedIndexes);
}

function testAdjust11() {
  var sm = createSelectionModel(20);

  sm.leadIndex = sm.anchorIndex = 10;
  sm.selectAll();

  adjust(sm, 5, 20, 10);

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(0, 4), sm.selectedIndexes);
}

function testAdjust12() {
  var sm = createSelectionModel(20, true);

  sm.selectAll();
  sm.leadIndex = sm.anchorIndex = 10;

  adjust(sm, 5, 20, 10);

  assertEquals(0, sm.leadIndex, 'lead');
  assertEquals(0, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(0, 4), sm.selectedIndexes);
}

function testAdjust13() {
  var sm = createSelectionModel(20, true);

  sm.selectAll();
  sm.leadIndex = sm.anchorIndex = 15;

  adjust(sm, 5, 5, 0);

  assertEquals(10, sm.leadIndex, 'lead');
  assertEquals(10, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(0, 14), sm.selectedIndexes);
}

function testAdjust14() {
  var sm = createSelectionModel(5, true);

  sm.selectedIndexes = [2, 3];
  sm.leadIndex = sm.anchorIndex = 3;

  adjust(sm, 2, 2, 0);

  assertEquals(2, sm.leadIndex, 'lead');
  assertEquals(2, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(2, 2), sm.selectedIndexes);
}

function testAdjust15() {
  var sm = createSelectionModel(7, true);

  sm.selectedIndexes = [1, 3, 5];
  sm.leadIndex = sm.anchorIndex = 1;

  adjust(sm, 1, 1, 0);
  adjust(sm, 2, 1, 0);
  adjust(sm, 3, 1, 0);

  assertEquals(3, sm.leadIndex, 'lead');
  assertEquals(3, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(3, 3), sm.selectedIndexes);
}

function testAdjust16() {
  var sm = createSelectionModel(7, true);

  sm.selectedIndexes = [1, 3, 5];
  sm.leadIndex = sm.anchorIndex = 3;

  adjust(sm, 1, 1, 0);
  adjust(sm, 2, 1, 0);
  adjust(sm, 3, 1, 0);

  assertEquals(3, sm.leadIndex, 'lead');
  assertEquals(3, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(3, 3), sm.selectedIndexes);
}

function testAdjust17() {
  var sm = createSelectionModel(7, true);

  sm.selectedIndexes = [1, 3, 5];
  sm.leadIndex = sm.anchorIndex = 5;

  adjust(sm, 1, 1, 0);
  adjust(sm, 2, 1, 0);
  adjust(sm, 3, 1, 0);

  assertEquals(3, sm.leadIndex, 'lead');
  assertEquals(3, sm.anchorIndex, 'anchor');
  assertArrayEquals(range(3, 3), sm.selectedIndexes);
}

function testLeadAndAnchor1() {
  var sm = createSelectionModel(20, true);

  sm.selectAll();
  sm.leadIndex = sm.anchorIndex = 10;

  assertEquals(10, sm.leadIndex, 'lead');
  assertEquals(10, sm.anchorIndex, 'anchor');
}

function testLeadAndAnchor2() {
  var sm = createSelectionModel(20, true);

  sm.leadIndex = sm.anchorIndex = 10;
  sm.selectAll();

  assertEquals(0, sm.leadIndex, 'lead');
  assertEquals(0, sm.anchorIndex, 'anchor');
}

function testSelectAll() {
  var sm = createSelectionModel(10);

  var changes = null;
  sm.addEventListener('change', function(e) {
    changes = e.changes;
  });

  sm.selectAll();

  assertArrayEquals(range(0, 9), sm.selectedIndexes);
  assertArrayEquals(range(0, 9), changes.map(function(change) {
    return change.index;
  }));
}

function testSelectAllOnEmptyList() {
  var sm = createSelectionModel(0);

  var changes = null;
  sm.addEventListener('change', function(e) {
    changes = e.changes;
  });

  sm.selectAll();

  assertArrayEquals([], sm.selectedIndexes);
  assertEquals(null, changes);
}
