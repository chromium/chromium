// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from 'chrome://resources/js/assert.m.js';
import {alphabeticalCompare} from 'chrome://scanning/scanning_app_util.js';
import {flushTasks} from '../../test_util.m.js';

/**
 * @param {!Array} arr
 * @param {!function(T): U} conversionFn
 * @template T
 * @template U
 */
export function assertOrderedAlphabetically(arr, conversionFn = (val) => val) {
  for (let i = 0; i < arr.length - 1; i++) {
    // |alphabeticalCompare| will return -1 if the first argument is less than
    // the second and 0 if the two arguments are equal.
    assert(
        alphabeticalCompare(conversionFn(arr[i]), conversionFn(arr[i + 1])) <=
        0);
  }
}

/**
 * @param {!mojoBase.mojom.UnguessableToken} id
 * @param {string} displayName
 * @return {!chromeos.scanning.mojom.Scanner}
 */
export function createScanner(id, displayName) {
  return {id, 'displayName': strToMojoString16(displayName)};
}

/**
 * @param {number} type
 * @param {string} name
 * @param {!Array<chromeos.scanning.mojom.PageSize>} pageSizes
 * @return {!chromeos.scanning.mojom.ScanSource}
 */
export function createScannerSource(type, name, pageSizes) {
  return /** @type {!chromeos.scanning.mojom.ScanSource} */ (
      {type, name, pageSizes});
}

/**
 * Converts a JS string to a mojo_base::mojom::String16 object.
 * @param {string} str
 * @return {!mojoBase.mojom.String16}
 */
function strToMojoString16(str) {
  let arr = [];
  for (var i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }

  return {data: arr};
}

/**
 * @param {!HTMLSelectElement} select
 * @param {?string} value
 * @param {?number} selectedIndex
 * @return {!Promise}
 */
export function changeSelect(select, value, selectedIndex) {
  if (value) {
    select.value = value;
  }
  if (selectedIndex) {
    select.selectedIndex = selectedIndex;
  }
  select.dispatchEvent(new CustomEvent('change'));
  return flushTasks();
}
