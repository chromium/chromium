// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './realtime_cpu_chart.js';
import './routine_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CpuUsage, RoutineName, SystemDataProviderInterface} from './diagnostics_types.js';
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
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.observeCpuUsage_();
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

  /**
   * @param {number} percentUsageSystem
   * @param {number} percentUsageUser
   * @private
   */
  computeCurrentlyUsing_(percentUsageSystem, percentUsageUser) {
    return percentUsageSystem + percentUsageUser;
  },
});
