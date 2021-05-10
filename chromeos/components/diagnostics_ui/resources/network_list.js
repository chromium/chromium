// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './connectivity_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './network_card.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NetworkGuidInfo, NetworkHealthProviderInterface} from './diagnostics_types.js'
import {getNetworkHealthProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'network-list' is responsible for displaying Ethernet, Cellular,
 *  and WiFi networks.
 */
Polymer({
  is: 'network-list',

  _template: html`{__html_template__}`,

  /**
   * @private {?NetworkHealthProviderInterface}
   */
  networkHealthProvider_: null,

  properties: {
    /** @type {boolean} */
    isTestRunning: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /** @private {Array<?string>} */
    otherNetworkGuids_: {
      type: Array,
      value: () => [],
    },

    /** @private {string} */
    activeGuid_: {
      type: String,
      value: '',
    },
  },

  /** @override */
  created() {
    this.networkHealthProvider_ = getNetworkHealthProvider();
    this.observeNetworkList_();
  },

  /** @private */
  observeNetworkList_() {
    // Calling observeNetworkList will trigger onNetworkListChanged.
    this.networkHealthProvider_.observeNetworkList(this);
  },

  /**
   * Implements NetworkListObserver.onNetworkListChanged
   * @param {!NetworkGuidInfo} networkGuidInfo
   */
  onNetworkListChanged(networkGuidInfo) {
    // The connectivity-card is responsible for displaying the active network
    // so we need to filter out the activeGuid to avoid displaying a
    // a network-card for it.
    this.otherNetworkGuids_ = networkGuidInfo.networkGuids.filter(
        guid => guid !== networkGuidInfo.activeGuid);
    this.activeGuid_ = networkGuidInfo.activeGuid || '';
  },
});
