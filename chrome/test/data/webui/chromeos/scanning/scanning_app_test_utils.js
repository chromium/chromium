// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './scanning_mojom_imports.js';

import {ColorMode, PageSize} from 'chrome://scanning/scanning.mojom-webui.js';
import {alphabeticalCompare} from 'chrome://scanning/scanning_app_util.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

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
    assertTrue(
        alphabeticalCompare(conversionFn(arr[i]), conversionFn(arr[i + 1])) <=
        0);
  }
}

/**
 * @param {!mojoBase.mojom.UnguessableToken} id
 * @param {string} displayName
 * @return {!Scanner}
 */
export function createScanner(id, displayName) {
  return {id, 'displayName': strToMojoString16(displayName)};
}

/**
 * @param {number} type
 * @param {string} name
 * @param {!Array<PageSize>} pageSizes
 * @param {!Array<ColorMode>} colorModes
 * @param {!Array<number>} resolutions
 * @return {!ScanSource}
 */
export function createScannerSource(
    type, name, pageSizes, colorModes, resolutions) {
  return /** @type {!ScanSource} */ (
      {type, name, pageSizes, colorModes, resolutions});
}

/**
 * Converts a JS string to a mojo_base::mojom::String16 object.
 * @param {string} str
 * @return {!mojoBase.mojom.String16}
 */
function strToMojoString16(str) {
  const arr = [];
  for (let i = 0; i < str.length; i++) {
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
  } else if (selectedIndex !== null) {
    select.selectedIndex = selectedIndex;
  } else {
    return Promise.reject();
  }

  select.dispatchEvent(new CustomEvent('change'));
  return flushTasks();
}

/** @typedef {function(string, {media: string, matches: boolean})} */
let MediaQueryListEventListener;

/**
 * Fake MediaQueryList for mocking behavior of |window.matchMedia|.
 * @extends {EventTarget}
 * @implements {MediaQueryList}
 * @suppress {checkTypes} Type checker incorrectly states class cannot be
 * extended.
 */
export class FakeMediaQueryList extends EventTarget {
  constructor(media) {
    super();
    /** @type {string} */
    this.media_ = media;
    /** @type {boolean} */
    this.matches_ = false;
    /** @type {?MediaQueryListEventListener} */
    this.listener_ = null;
  }

  /** @param {!Function} listener */
  addListener(listener) {
    this.listener_ = listener;
  }

  /** @param {!Function} listener */
  removeListener(listener) {
    this.listener_ = null;
  }

  onchange() {
    if (!this.listener_) {
      return;
    }

    this.listener_(new window.MediaQueryListEvent(
        'change', {media: this.media_, matches: this.matches_}));
  }

  /** @return {string} */
  get media() {
    return this.media_;
  }

  /** @return {boolean} */
  get matches() {
    return this.matches_;
  }

  /** @param {boolean} matches */
  set matches(matches) {
    if (this.matches_ === matches) {
      return;
    }

    this.matches_ = matches;
    this.onchange();
  }
}
