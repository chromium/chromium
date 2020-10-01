// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './percent_bar_chart.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MemoryUsage, SystemDataProviderInterface} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import './strings.js';

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

  properties: {
    /** @private {!MemoryUsage} */
    memoryUsage_: {
      type: Object,
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.observeMemoryUsage_();
  },

  /** @private */
  observeMemoryUsage_() {
    this.systemDataProvider_.observeMemoryUsage(this);
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
    return memoryUsage.total_memory_kib -
        memoryUsage.available_memory_kib;
  }
});
