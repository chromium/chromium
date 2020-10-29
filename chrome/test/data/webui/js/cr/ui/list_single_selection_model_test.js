// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assertEquals, assertArrayEquals} from '../../../chai_assert.js';
// #import {ListSingleSelectionModel} from 'chrome://resources/js/cr/ui/list_single_selection_model.m.js';
// #import {adjust} from './list_selection_model_test_util.m.js';
// clang-format on

/**
 * @param {number} len size of the selection model.
 * @param {boolean=} dependentLeadItem inverse value for `independentLeadItem_`
 *     defaults to true.
 * @return {!cr.ui.ListSingleSelectionModel}
 */
function createSelectionModel(len, dependentLeadItem) {
  var sm = new cr.ui.ListSingleSelectionModel(len);
  sm.independentLeadItem_ = !dependentLeadItem;
  return sm;
}

/* #export */ function testAdjust1() {
  var sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 100;
  adjust(sm, 0, 10, 0);

  assertEquals(90, sm.leadIndex);
  assertEquals(90, sm.anchorIndex);
  assertEquals(90, sm.selectedIndex);
}

/* #export */ function testAdjust2() {
  var sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 50;
  adjust(sm, 60, 10, 0);

  assertEquals(50, sm.leadIndex);
  assertEquals(50, sm.anchorIndex);
  assertEquals(50, sm.selectedIndex);
}

/* #export */ function testAdjust3() {
  var sm = createSelectionModel(200);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 100;
  adjust(sm, 0, 0, 10);

  assertEquals(110, sm.leadIndex);
  assertEquals(110, sm.anchorIndex);
  assertEquals(110, sm.selectedIndex);
}

/* #export */ function testAdjust4() {
  var sm = createSelectionModel(100);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 99;

  adjust(sm, 99, 1, 0);

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals([], sm.selectedIndexes);
}

/* #export */ function testAdjust5() {
  var sm = createSelectionModel(1);

  sm.leadIndex = sm.anchorIndex = sm.selectedIndex = 0;

  adjust(sm, 0, 0, 10);

  assertEquals(10, sm.leadIndex, 'lead');
  assertEquals(10, sm.anchorIndex, 'anchor');
  assertArrayEquals([10], sm.selectedIndexes);
}

/* #export */ function testSelectedIndex1() {
  var sm = createSelectionModel(100, true);

  sm.selectedIndex = 99;

  assertEquals(99, sm.leadIndex, 'lead');
  assertEquals(99, sm.anchorIndex, 'anchor');
  assertArrayEquals([99], sm.selectedIndexes);
}

/* #export */ function testLeadIndex1() {
  var sm = createSelectionModel(100);

  sm.leadIndex = 99;

  assertEquals(99, sm.leadIndex, 'lead');
  assertEquals(99, sm.anchorIndex, 'anchor');
  assertArrayEquals([], sm.selectedIndexes);
}

/* #export */ function testLeadIndex2() {
  var sm = createSelectionModel(100, true);

  sm.leadIndex = 99;

  assertEquals(-1, sm.leadIndex, 'lead');
  assertEquals(-1, sm.anchorIndex, 'anchor');
  assertArrayEquals([], sm.selectedIndexes);
}

Object.assign(window, {
  testAdjust1,
  testAdjust2,
  testAdjust3,
  testAdjust4,
  testAdjust5,
  testSelectedIndex1,
  testLeadIndex1,
  testLeadIndex2,
});
