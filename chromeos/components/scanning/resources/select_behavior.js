// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Helper functions for custom elements that implement a scan setting using a
 * single select element.
 * @polymerBehavior
 */
export const SelectBehavior = {
  properties: {
    /** @type {boolean} */
    disabled: Boolean,
  },

  /**
   * @param {!Array} arr
   * @param {!function(string, string): number} compareFn
   * @param {!function(T): string} conversionFn
   * @template T
   */
  customSort(arr, compareFn, conversionFn = (val) => val) {
    return arr.sort((a, b) => compareFn(conversionFn(a), conversionFn(b)));
  },
};
