// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './percent_bar_chart.js';
import './routine_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BatteryChargeStatus, BatteryHealth, BatteryInfo, RoutineType, SystemDataProviderInterface} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {mojoString16ToString} from './mojo_utils.js';

/**
 * @fileoverview
 * 'battery-status-card' shows information about battery status.
 */
Polymer({
  is: 'battery-status-card',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  /**
   * Receiver responsible for observing battery charge status.
   * @private {
   *  ?chromeos.diagnostics.mojom.BatteryChargeStatusObserverReceiver}
   */
  batteryChargeStatusObserverReceiver_: null,

  /**
   * Receiver responsible for observing battery health.
   * @private {
   *  ?chromeos.diagnostics.mojom.BatteryHealthObserverReceiver}
   */
  batteryHealthObserverReceiver_: null,

  properties: {
    /** @private {!BatteryChargeStatus} */
    batteryChargeStatus_: {
      type: Object,
    },

    /** @private {!BatteryHealth} */
    batteryHealth_: {
      type: Object,
    },

    /** @private {!BatteryInfo} */
    batteryInfo_: {
      type: Object,
    },

    /** @private {!Array<!RoutineType>} */
    routines_: {
      type: Array,
      computed:
          'getCurrentPowerRoutines_(batteryChargeStatus_.powerAdapterStatus)',
    },

    /** @protected {string} */
    powerTimeString_: {
      type: String,
      computed: 'decodeString16_(batteryChargeStatus_.powerTime)',
    },

    /** @type {boolean} */
    isTestRunning: {
      type: Boolean,
      notify: true,
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.fetchBatteryInfo_();
    this.observeBatteryChargeStatus_();
    this.observeBatteryHealth_();
  },

  /** @override */
  detached() {
    this.batteryChargeStatusObserverReceiver_.$.close();
    this.batteryHealthObserverReceiver_.$.close();
  },

  /** @private */
  fetchBatteryInfo_() {
    this.systemDataProvider_.getBatteryInfo().then((result) => {
      this.onBatteryInfoReceived_(result.batteryInfo);
    });
  },

  /**
   * @param {!BatteryInfo} batteryInfo
   * @private
   */
  onBatteryInfoReceived_(batteryInfo) {
    this.batteryInfo_ = batteryInfo;
  },

  /** @private */
  observeBatteryChargeStatus_() {
    this.batteryChargeStatusObserverReceiver_ =
        new chromeos.diagnostics.mojom.BatteryChargeStatusObserverReceiver(
            /**
             * @type {!chromeos.diagnostics.mojom.
             *        BatteryChargeStatusObserverInterface}
             */
            (this));

    this.systemDataProvider_.observeBatteryChargeStatus(
        this.batteryChargeStatusObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Implements BatteryChargeStatusObserver.onBatteryChargeStatusUpdated()
   * @param {!BatteryChargeStatus} batteryChargeStatus
   */
  onBatteryChargeStatusUpdated(batteryChargeStatus) {
    this.batteryChargeStatus_ = batteryChargeStatus;
  },

  /** @private */
  observeBatteryHealth_() {
    this.batteryHealthObserverReceiver_ =
        new chromeos.diagnostics.mojom.BatteryHealthObserverReceiver(
            /**
             * @type {!chromeos.diagnostics.mojom.
             *        BatteryHealthObserverInterface}
             */
            (this));

    this.systemDataProvider_.observeBatteryHealth(
        this.batteryHealthObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Get an array of currently relevant routines based on power adaptor status
   * @param {!chromeos.diagnostics.mojom.ExternalPowerSource} powerAdapterStatus
   * @return {!Array<!RoutineType>}
   * @private
   */
  getCurrentPowerRoutines_(powerAdapterStatus) {
    return powerAdapterStatus ===
            chromeos.diagnostics.mojom.ExternalPowerSource.kDisconnected ?
        [chromeos.diagnostics.mojom.RoutineType.kBatteryDischarge] :
        [chromeos.diagnostics.mojom.RoutineType.kBatteryCharge];
  },

  /**
   * Converts utf16 to a readable string.
   * @param {!mojoBase.mojom.String16} str16
   * @return {string}
   * @private
   */
  decodeString16_(str16) {
    return mojoString16ToString(str16);
  },

  /**
   * Implements BatteryHealthObserver.onBatteryHealthUpdated()
   * @param {!BatteryHealth} batteryHealth
   */
  onBatteryHealthUpdated(batteryHealth) {
    this.batteryHealth_ = batteryHealth;
  },

  /** @protected */
  getDesignedFullCharge_() {
    return loadTimeData.getStringF(
        'batteryChipText', this.batteryHealth_.chargeFullDesignMilliampHours);
  },

  /** @protected */
  getBatteryHealth_() {
    const MAX_PERCENTAGE = 100;
    const batteryWearPercentage =
        Math.min(this.batteryHealth_.batteryWearPercentage, MAX_PERCENTAGE);
    return loadTimeData.getStringF('batteryHealthText', batteryWearPercentage);
  },

  /** @protected */
  getCurrentNow_() {
    return loadTimeData.getStringF(
        'currentNowText', this.batteryChargeStatus_.currentNowMilliamps);
  },

  /** @protected */
  getRunTestsButtonText_() {
    return loadTimeData.getString(
        this.batteryChargeStatus_.powerAdapterStatus ===
                chromeos.diagnostics.mojom.ExternalPowerSource.kDisconnected ?
            'runBatteryDischargeTestText' :
            'runBatteryChargeTestText')
  }
});
