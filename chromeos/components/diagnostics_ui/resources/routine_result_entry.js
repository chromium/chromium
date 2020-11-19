// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './text_badge.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {RoutineName, RoutineResult, StandardRoutineResult} from './diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';
import {BadgeType} from './text_badge.js';

/**
 * Resolves an enum value to the string name. This is used temporarily to
 * provide a human readable string until the final mapping of enum values to
 * localized strings is finalized.
 * TODO(zentaro): Remove this function when strings are finalized.
 * @param {!Object} enumType
 * @param {number} enumValue
 * @return {string}
 */
export function lookupEnumValueName(enumType, enumValue) {
  for (const [key, value] of Object.entries(enumType)) {
    if (value === enumValue) {
      return key;
    }
  }

  // Values should always be found in the enum.
  assert(false);
  return '';
}

/**
 * @fileoverview
 * 'routine-result-entry' shows the status of a single test routine.
 */
Polymer({
  is: 'routine-result-entry',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!ResultStatusItem} */
    item: {
      type: Object,
    },

    /** @private */
    routineName_: {
      type: String,
      computed: 'getRoutineName_(item.routine)',
    },
  },

  /**
   * Get the string name for the routine.
   * TODO(zentaro): Replace with a mapping to localized string when they are
   * finalized.
   * @param {!RoutineName} routine
   * @return {string}
   */
  getRoutineName_(routine) {
    return lookupEnumValueName(RoutineName, routine);
  },

  /**
   * @param {!RoutineResult} result
   * @return {!StandardRoutineResult}
   */
  getSimpleResult_(result) {
    assert(result);

    if (result.hasOwnProperty('simpleResult')) {
      // Ideally we would just return assert(result.simpleResult) but enum
      // value 0 fails assert.
      return /** @type {!StandardRoutineResult} */ (result.simpleResult);
    }

    if (result.hasOwnProperty('batteryRateResult')) {
      return /** @type {!StandardRoutineResult} */ (
          result.batteryRateResult.result);
    }

    assertNotReached();
  },

  /**
   * @protected
   */
  getBadgeText_() {
    // TODO(joonbug): Localize this string.
    if (this.item.progress === ExecutionProgress.kRunning) {
      return 'RUNNING';
    }

    if (this.item.result &&
        this.getSimpleResult_(this.item.result) ===
            StandardRoutineResult.kTestPassed) {
      return 'SUCCESS';
    }

    return 'FAILED';
  },

  /**
   * @protected
   */
  getBadgeType_() {
    // TODO(joonbug): Localize this string.
    if (this.item.progress === ExecutionProgress.kRunning) {
      return BadgeType.DEFAULT;
    }

    if (this.item.result &&
        this.getSimpleResult_(this.item.result) ===
            StandardRoutineResult.kTestPassed) {
      return BadgeType.SUCCESS;
    }
    return BadgeType.ERROR;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isTestStarted_() {
    return this.item.progress !== ExecutionProgress.kNotStarted;
  },

  /** @override */
  created() {},
});
