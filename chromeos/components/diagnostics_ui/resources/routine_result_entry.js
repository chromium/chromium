// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './text_badge.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
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
  // TODO(michaelcheco): Replace unlocalized strings.
  switch (routineType) {
    case RoutineType.kBatteryCharge:
      return loadTimeData.getString('batteryChargeRoutineText');
    case RoutineType.kBatteryDischarge:
      return loadTimeData.getString('batteryDischargeRoutineText');
    case RoutineType.kCaptivePortal:
      return 'Captive Portal';
    case RoutineType.kCpuCache:
      return loadTimeData.getString('cpuCacheRoutineText');
    case RoutineType.kCpuStress:
      return loadTimeData.getString('cpuStressRoutineText');
    case RoutineType.kCpuFloatingPoint:
      return loadTimeData.getString('cpuFloatingPointAccuracyRoutineText');
    case RoutineType.kCpuPrime:
      return loadTimeData.getString('cpuPrimeSearchRoutineText');
    case RoutineType.kDnsLatency:
      return 'DNS Latency';
    case RoutineType.kDnsResolution:
      return 'DNS Resolution';
    case RoutineType.kDnsResolverPresent:
      return 'DNS Resolver Present';
    case RoutineType.kGatewayCanBePinged:
      return 'Gateway can be Pinged';
    case RoutineType.kHasSecureWiFiConnection:
      return 'Secure WiFi Connection';
    case RoutineType.kHttpFirewall:
      return 'HTTP Firewall';
    case RoutineType.kHttpsFirewall:
      return 'HTTPS Firewall';
    case RoutineType.kHttpsLatency:
      return 'HTTPS Latency';
    case RoutineType.kLanConnectivity:
      return 'Lan Connectivity';
    case RoutineType.kMemory:
      return loadTimeData.getString('memoryRoutineText');
    case RoutineType.kSignalStrength:
      return 'Signal Strength';
    default:
      // Values should always be found in the enum.
      assert(false);
      return '';
  }
}

/**
 * @param {!RoutineResult} result
 * @return {?StandardRoutineResult}
 */
export function getSimpleResult(result) {
  if (!result) {
    return null;
  }

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
  return null;
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

    /** @private {!BadgeType} */
    badgeType_: {
      type: String,
      value: BadgeType.QUEUED,
    },

    /** @private {string} */
    badgeText_: {
      type: String,
      value: '',
    },
  },

  observers: ['entryStatusChanged_(item.progress, item.result)'],

  /** @override */
  attached() {
    IronA11yAnnouncer.requestAvailability();
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
   * @private
   */
  entryStatusChanged_() {
    switch (this.item.progress) {
      case ExecutionProgress.kNotStarted:
        this.setBadgeTypeAndText_(
            BadgeType.QUEUED, loadTimeData.getString('testQueuedBadgeText'));
        break;
      case ExecutionProgress.kRunning:
        this.setBadgeTypeAndText_(
            BadgeType.RUNNING, loadTimeData.getString('testRunningBadgeText'));
        this.announceRoutineStatus_();
        break;
      case ExecutionProgress.kCancelled:
        this.setBadgeTypeAndText_(
            BadgeType.STOPPED, loadTimeData.getString('testStoppedBadgeText'));
        this.announceRoutineStatus_();
        break;
      case ExecutionProgress.kCompleted:
        const testPassed = this.item.result &&
            getSimpleResult(this.item.result) ===
                StandardRoutineResult.kTestPassed;
        const badgeType = testPassed ? BadgeType.SUCCESS : BadgeType.ERROR;
        const badgeText = loadTimeData.getString(
            testPassed ? 'testSucceededBadgeText' : 'testFailedBadgeText');
        this.setBadgeTypeAndText_(badgeType, badgeText);
        this.announceRoutineStatus_();
        break;
      default:
        assertNotReached();
    }
  },

  /**
   * @param {!BadgeType} badgeType
   * @param {string} badgeText
   * @private
   */
  setBadgeTypeAndText_(badgeType, badgeText) {
    this.setProperties({badgeType_: badgeType, badgeText_: badgeText});
  },

  /** @override */
  created() {},

  /** @private */
  announceRoutineStatus_() {
    this.fire(
        'iron-announce', {text: this.routineType_ + ' - ' + this.badgeText_});
  },
});
