// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {ListSelectionModel} from 'chrome://resources/js/cr/ui/list_selection_model.m.js';
// #import {ListSingleSelectionModel} from 'chrome://resources/js/cr/ui/list_single_selection_model.m.js';
// clang-format on

/**
 * Creates an array spanning a range of integer values.
 * @param {number} start The first number in the range.
 * @param {number} end The last number in the range inclusive.
 * @return {!Array<number>}
 */
/* #export */ function range(start, end) {
  var a = [];
  for (var i = start; i <= end; i++) {
    a.push(i);
  }
  return a;
}

/**
 * Modifies a selection model.
 * @param {!cr.ui.ListSelectionModel|!cr.ui.ListSingleSelectionModel} model The
 * selection model to adjust.
 * @param {number} index Starting index of the edit.
 * @param {number} removed Number of entries to remove from the list.
 * @param {number} added Number of entries to add to the list.
 */
/* #export */ function adjust(model, index, removed, added) {
  var permutation = [];
  for (var i = 0; i < index; i++) {
    permutation.push(i);
  }
  for (var i = 0; i < removed; i++) {
    permutation.push(-1);
  }
  for (var i = index + removed; i < model.length; i++) {
    permutation.push(i - removed + added);
  }
  model.adjustLength(model.length - removed + added);
  model.adjustToReordering(permutation);
}
