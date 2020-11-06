// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @type {number} */
const REQUIRED_NUM_OPTIONS = 2;

/**
 * Helper functions for custom elements that implement a scan setting using a
 * single select element.
 * @polymerBehavior
 */
export const SelectBehavior = {
  properties: {
    /** Controls whether the select element is disabled. */
    disabled: {
      type: Boolean,
      computed: 'computeDisabled_(numOptions_, settingsDisabled)',
    },

    /**
     * Indicates whether all scan settings have been disabled by the parent
     * element.
     */
    settingsDisabled: Boolean,

    /**
     * The number of options in the select element.
     * @type {number}
     * @private
     */
    numOptions_: {
      type: Number,
      value: 0,
    },
  },

  /**
   * Called by the custom element when the number of options in its select
   * element changes.
   * @param {number} numOptions
   */
  onNumOptionsChange(numOptions) {
    this.numOptions_ = numOptions;
  },

  /**
   * Determines whether the select element should be disabled based on its
   * number of options and whether settings are disabled.
   * @param {number} numOptions
   * @param {boolean} settingsDisabled
   * @return {boolean}
   * @private
   */
  computeDisabled_(numOptions, settingsDisabled) {
    return numOptions < REQUIRED_NUM_OPTIONS || settingsDisabled;
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
