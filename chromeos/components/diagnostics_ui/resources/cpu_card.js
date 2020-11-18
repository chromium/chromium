// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './realtime_cpu_chart.js';
import './routine_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CpuUsage, RoutineName, SystemDataProviderInterface, SystemInfo} from './diagnostics_types.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'cpu-card' shows information about the CPU.
 */
Polymer({
  is: 'cpu-card',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  /**
   * Receiver responsible for observing CPU usage.
   * @private {?chromeos.diagnostics.mojom.CpuUsageObserverReceiver}
   */
  cpuUsageObserverReceiver_: null,

  properties: {
    /** @private {!Array<!RoutineName>} */
    routines_: {
      type: Array,
      value: () => {
        return [
          RoutineName.kCpuStress,
          RoutineName.kCpuCache,
          RoutineName.kFloatingPoint,
          RoutineName.kPrimeSearch,
        ];
      }
    },

    /** @private {!CpuUsage} */
    cpuUsage_: {
      type: Object,
    },

    /** @private {string} */
    cpuChipInfo_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.observeCpuUsage_();
    this.fetchSystemInfo_();
  },

  /** @override */
  detached() {
    this.cpuUsageObserverReceiver_.$.close();
  },

  /** @private */
  observeCpuUsage_() {
    this.cpuUsageObserverReceiver_ =
        new chromeos.diagnostics.mojom.CpuUsageObserverReceiver(
            /**
             * @type {!chromeos.diagnostics.mojom.CpuUsageObserverInterface}
             */
            (this));

    this.systemDataProvider_.observeCpuUsage(
        this.cpuUsageObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Implements CpuUsageObserver.onCpuUsageUpdated.
   * @param {!CpuUsage} cpuUsage
   */
  onCpuUsageUpdated(cpuUsage) {
    this.cpuUsage_ = cpuUsage;
  },

  /** @protected */
  getCurrentlyUsing_() {
    const MAX_PERCENTAGE = 100;
    const usagePercentage = Math.min(
        (this.cpuUsage_.percentUsageSystem + this.cpuUsage_.percentUsageUser),
        MAX_PERCENTAGE);
    return loadTimeData.getStringF('cpuUsageText', usagePercentage);
  },

  /** @private */
  fetchSystemInfo_() {
    this.systemDataProvider_.getSystemInfo().then((result) => {
      this.onSystemInfoReceived_(result.systemInfo);
    });
  },

  /**
   * @param {!SystemInfo} systemInfo
   * @private
   */
  onSystemInfoReceived_(systemInfo) {
    // TODO(michaelcheco): Update when number of cores is added to the api.
    this.cpuChipInfo_ = loadTimeData.getStringF(
        'cpuChipText', systemInfo.cpuModelName, systemInfo.cpuThreadsCount);
  },

  /** @protected */
  getCpuTemp_() {
    return loadTimeData.getStringF(
        'cpuTempText', this.cpuUsage_.averageCpuTempCelsius);
  },
});
