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
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MemoryUsage, RoutineType, SystemDataProviderInterface} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'memory-card' shows information about system memory.
 */
Polymer({
  is: 'memory-card',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  /**
   * Receiver responsible for observing memory usage.
   * @private {
   *  ?chromeos.diagnostics.mojom.MemoryUsageObserverReceiver}
   */
  memoryUsageObserverReceiver_: null,

  properties: {
    /** @private {!Array<!RoutineType>} */
    routines_: {
      type: Array,
      value: () => {
        return [
          chromeos.diagnostics.mojom.RoutineType.kMemory,
        ];
      }
    },

    /** @private {!MemoryUsage} */
    memoryUsage_: {
      type: Object,
    },

    /** @type {boolean} */
    isTestRunning: {
      type: Boolean,
      notify: true,
    }
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.observeMemoryUsage_();
  },

  /** @override */
  detached() {
    this.memoryUsageObserverReceiver_.$.close();
  },

  /** @private */
  observeMemoryUsage_() {
    this.memoryUsageObserverReceiver_ =
        new chromeos.diagnostics.mojom.MemoryUsageObserverReceiver(
            /**
             * @type {!chromeos.diagnostics.mojom.MemoryUsageObserverInterface}
             */
            (this));

    this.systemDataProvider_.observeMemoryUsage(
        this.memoryUsageObserverReceiver_.$.bindNewPipeAndPassRemote());
  },

  /**
   * Implements MemoryUsageObserver.onMemoryUsageUpdated()
   * @param {!MemoryUsage} memoryUsage
   */
  onMemoryUsageUpdated(memoryUsage) {
    this.memoryUsage_ = memoryUsage;
  },

  /**
   * Calculates total used memory from MemoryUsage object.
   * @param {!MemoryUsage} memoryUsage
   * @return {number}
   * @private
   */
  getTotalUsedMemory_(memoryUsage) {
    return memoryUsage.totalMemoryKib - memoryUsage.availableMemoryKib;
  }
});
