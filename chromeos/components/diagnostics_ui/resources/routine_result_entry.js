// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './text_badge.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RoutineResult, RoutineType, StandardRoutineResult} from './diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from './routine_list_executor.js';
import {BadgeType} from './text_badge.js';

/**
 * Resolves a routine name to its corresponding localized string name.
 * @param {!RoutineType} routineType
 * @return {string}
 */
export function getRoutineType(routineType) {
  switch (routineType) {
    case chromeos.diagnostics.mojom.RoutineType.kBatteryCharge:
      return loadTimeData.getString('batteryChargeRoutineText');
    case chromeos.diagnostics.mojom.RoutineType.kBatteryDischarge:
      return loadTimeData.getString('batteryDischargeRoutineText');
    case chromeos.diagnostics.mojom.RoutineType.kCpuCache:
      return loadTimeData.getString('cpuCacheRoutineText');
    case chromeos.diagnostics.mojom.RoutineType.kCpuStress:
      return loadTimeData.getString('cpuStressRoutineText');
    case chromeos.diagnostics.mojom.RoutineType.kCpuFloatingPoint:
      return loadTimeData.getString('cpuFloatingPointAccuracyRoutineText');
    case chromeos.diagnostics.mojom.RoutineType.kCpuPrime:
      return loadTimeData.getString('cpuPrimeSearchRoutineText');
    case chromeos.diagnostics.mojom.RoutineType.kMemory:
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
    routineType_: {
      type: String,
      computed: 'getRunningRoutineString_(item.routine)',
    },
  },

  /**
   * Get the localized string name for the routine.
   * @param {!RoutineType} routine
   * @return {string}
   */
  getRunningRoutineString_(routine) {
    return loadTimeData.getStringF('routineEntryText', getRoutineType(routine));
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

    if (result.hasOwnProperty('powerResult')) {
      return /** @type {!StandardRoutineResult} */ (
          result.powerResult.simpleResult);
    }

    assertNotReached();
  },

  /**
   * @protected
   */
  getBadgeText_() {
    if (this.item.progress === ExecutionProgress.kRunning) {
      return loadTimeData.getString('testRunningBadgeText');
    }

    if (this.item.result &&
        this.getSimpleResult_(this.item.result) ===
            chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed) {
      return loadTimeData.getString('testSucceededBadgeText');
    }

    return loadTimeData.getString('testFailedBadgeText');
  },

  /**
   * @protected
   */
  getBadgeType_() {
    if (this.item.progress === ExecutionProgress.kRunning) {
      return BadgeType.DEFAULT;
    }

    if (this.item.result &&
        this.getSimpleResult_(this.item.result) ===
            chromeos.diagnostics.mojom.StandardRoutineResult.kTestPassed) {
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
