// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CpuUsage, SystemDataProviderInterface} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'cpu-card' shows information about the CPU.
 */
Polymer({
  is: 'cpu-card',

  _template: html`{__html_template__}`,

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  properties: {
    /** @private {!CpuUsage} */
    cpuUsage_: {
      type: Object,
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.observeCpuUsage_();
  },

  /** @private */
  observeCpuUsage_() {
    this.systemDataProvider_.observeCpuUsage(this);
  },

  /**
   * Implements CpuUsageObserver.onCpuUsageUpdated.
   * @param {!CpuUsage} cpuUsage
   */
  onCpuUsageUpdated(cpuUsage) {
    this.cpuUsage_ = cpuUsage;
  },

});
