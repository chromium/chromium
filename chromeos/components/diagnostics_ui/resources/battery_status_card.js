// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BatteryInfo, SystemDataProviderInterface} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'battery-status-card' shows information about battery status.
 */
Polymer({
  is: 'battery-status-card',

  _template: html`{__html_template__}`,

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  properties: {
    /** @private {!BatteryInfo} */
    batteryInfo_: {
      type: Object,
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.fetchBatteryInfo_();
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

});
