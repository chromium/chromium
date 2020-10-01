// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './percent_bar_chart.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, SystemDataProviderInterface} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import './strings.js';

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
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.fetchBatteryInfo_();
    this.observeBatteryChargeStatus_();
    this.observeBatteryHealth_();
  },

  /** @private */
  fetchBatteryInfo_() {
    this.systemDataProvider_.getBatteryInfo().then(
        this.onBatteryInfoReceived_.bind(this));
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
    this.systemDataProvider_.observeBatteryChargeStatus(this);
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
    this.systemDataProvider_.observeBatteryHealth(this);
  },

  /**
   * Implements BatteryHealthObserver.onBatteryHealthUpdated()
   * @param {!BatteryHealth} batteryHealth
   */
  onBatteryHealthUpdated(batteryHealth) {
    this.batteryHealth_ = batteryHealth;
  },

});
