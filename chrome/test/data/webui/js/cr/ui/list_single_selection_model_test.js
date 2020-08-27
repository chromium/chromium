// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createSelectionModel(len, opt_dependentLeadItem) {
  var sm = new cr.ui.ListSingleSelectionModel(len);
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
  var sm = createSelectionModel(100);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 99;

  adjust(sm, 99, 1, 0);

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals([], sm.selectedIndexes);
}

function testAdjust5() {
  var sm = createSelectionModel(1);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 0;

  adjust(sm, 0, 0, 10);

  assertEquals(10, sm.leadIndex, 'lead');
  assertEquals(10, sm.anchorIndex, 'anchor');
  assertArrayEquals([10], sm.selectedIndexes);
}

function testSelectedIndex1() {
  var sm = createSelectionModel(100, true);

  sm.selectedIndex = 99;

  assertEquals(99, sm.leadIndex, 'lead');
  assertEquals(99, sm.anchorIndex, 'anchor');
  assertArrayEquals([99], sm.selectedIndexes);
}

function testLeadIndex1() {
  var sm = createSelectionModel(100);

  sm.leadIndex = 99;

  assertEquals(99, sm.leadIndex, 'lead');
  assertEquals(99, sm.anchorIndex, 'anchor');
  assertArrayEquals([], sm.selectedIndexes);
}

function testLeadIndex2() {
  var sm = createSelectionModel(100, true);

  sm.leadIndex = 99;

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals([], sm.selectedIndexes);
}
