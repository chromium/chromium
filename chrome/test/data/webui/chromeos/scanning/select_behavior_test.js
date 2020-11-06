// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {alphabeticalCompare} from 'chrome://scanning/scanning_app_util.js';
import {SelectBehavior} from 'chrome://scanning/select_behavior.js';

import * as utils from './scanning_app_test_utils.js';

export function selectBehaviorTest() {
  /** @type {?TestSelectElement} */
  let testSelect = null;

  suiteSetup(function() {
    Polymer({
      is: 'test-select',

      _template: html`
      <select></select>
    `,

      behaviors: [SelectBehavior],

    });
  });

  setup(() => {
    testSelect = document.createElement('test-select');
    document.body.innerHTML = '';
    document.body.appendChild(testSelect);
  });

  teardown(() => {
    if (testSelect) {
      testSelect.remove();
    }
    testSelect = null;
  });

  test('SortWithConversionFunctionProvided', () => {
    testSelect.arr = [{value: 'C'}, {value: 'A'}, {value: 'B'}];
    const conversionFn = (item) => item.value;
    flush();
    testSelect.customSort(testSelect.arr, alphabeticalCompare, conversionFn);
    flush();
    utils.assertOrderedAlphabetically(testSelect.arr, conversionFn);
  });

  test('SortWithNoConversionFunction', () => {
    testSelect.arr = ['C', 'D', 'F', 'B', 'A'];
    flush();
    testSelect.customSort(testSelect.arr, alphabeticalCompare);
    flush();
    utils.assertOrderedAlphabetically(testSelect.arr);
  });

  test('SortWithDuplicateValues', () => {
    testSelect.arr = ['C', 'C', 'B', 'B', 'A'];
    flush();
    testSelect.customSort(testSelect.arr, alphabeticalCompare);
    flush();
    utils.assertOrderedAlphabetically(testSelect.arr);
  });
}
