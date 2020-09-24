// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SystemDataProviderInterface, SystemInfo} from './diagnostics_types.js'
import {getSystemDataProvider} from './mojo_interface_provider.js';


/**
 * @fileoverview
 * 'overview-card' shows an overview of system information such
 * as CPU type, version, board name, and memory.
 */
Polymer({
  is: 'overview-card',

  _template: html`{__html_template__}`,

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  properties: {
    /** @private {!SystemInfo} */
    systemInfo_: {
      type: Object,
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.fetchSystemInfo_();
  },

  /** @private */
  fetchSystemInfo_() {
    this.systemDataProvider_.getSystemInfo().then(
        this.onSystemInfoReceived_.bind(this));
  },

  /**
   * @param {!SystemInfo} systemInfo
   * @private
   */
  onSystemInfoReceived_(systemInfo) {
    this.systemInfo_ = systemInfo;
  },

});
