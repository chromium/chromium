// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MemoryUsage, SystemDataProviderInterface} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'memory-card' shows information about system memory.
 */
Polymer({
  is: 'memory-card',

  _template: html`{__html_template__}`,

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

});
