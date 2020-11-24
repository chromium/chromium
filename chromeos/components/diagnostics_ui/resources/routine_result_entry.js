// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './text_badge.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {RoutineName, RoutineResult, StandardRoutineResult} from './diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';
import {BadgeType} from './text_badge.js';

/**
 * Resolves a routine name to its corresponding localized string name.
 * @param {!RoutineName} routineName
 * @return {string}
 */
export function getRoutineName(routineName) {
  switch (routineName) {
    case RoutineName.kCharge:
      return loadTimeData.getString('batteryChargeRoutineText');
    case RoutineName.kDischarge:
      return loadTimeData.getString('batteryDischargeRoutineText');
    case RoutineName.kCpuCache:
      return loadTimeData.getString('cpuCacheRoutineText');
    case RoutineName.kCpuStress:
      return loadTimeData.getString('cpuStressRoutineText');
    case RoutineName.kFloatingPoint:
      return loadTimeData.getString('cpuFloatingPointAccuracyRoutineText');
    case RoutineName.kPrimeSearch:
      return loadTimeData.getString('cpuPrimeSearchRoutineText');
    case RoutineName.kMemory:
      return loadTimeData.getString('memoryRoutineText');
    default:
      // Values should always be found in the enum.
      assert(false);
      return '';
  }
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
      computed: 'getRunningRoutineString_(item.routine)',
    },
  },
  /**
   * Get the localized string name for the routine.
   * @param {!RoutineName} routine
   * @return {string}
   */
  getRunningRoutineString_(routine) {
    return loadTimeData.getStringF('routineNameText', getRoutineName(routine));
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
