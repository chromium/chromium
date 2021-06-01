// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cellular_info.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './ethernet_info.js';
import './wifi_info.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Network, NetworkHealthProviderInterface, NetworkStateObserverInterface, NetworkStateObserverReceiver, NetworkType} from './diagnostics_types.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'network-info' is responsible for observing a network guid and
 * displaying specialized data points for a supported network type
 * (Ethernet, WiFi, Cellular).
 */
Polymer({
  is: 'network-info',

  _template: html`{__html_template__}`,

  /**
   * @private {?NetworkHealthProviderInterface}
   */
  networkHealthProvider_: null,

  /**
   * Receiver responsible for observing a single active network connection.
   * @private {?NetworkStateObserverReceiver}
   */
  networkStateObserverReceiver_: null,

  properties: {
    /** @type {string} */
    guid: {
      type: String,
      value: '',
    },

    /** @private {!Network} */
    network_: {
      type: Object,
    },
  },

  observers: ['observeNetwork_(guid)'],

  /** @override */
  created() {
    this.networkHealthProvider_ = getNetworkHealthProvider();
  },

  /** @private */
  observeNetwork_() {
    if (!this.guid) {
      return;
    }

    if (this.networkStateObserverReceiver_) {
      this.networkStateObserverReceiver_.$.close();
      this.networkStateObserverReceiver_ = null;
    }

    this.networkStateObserverReceiver_ = new NetworkStateObserverReceiver(
        /**
         * @type {!NetworkStateObserverInterface}
         */
        (this));

    this.networkHealthProvider_.observeNetwork(
        this.networkStateObserverReceiver_.$.bindNewPipeAndPassRemote(),
        this.guid);
  },

  /**
   * Implements NetworkStateObserver.onNetworkStateChanged
   * @param {!Network} network
   */
  onNetworkStateChanged(network) {
    this.network_ = network;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isWifiNetwork_() {
    return this.network_.type === NetworkType.kWiFi;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isCellularNetwork_() {
    return this.network_.type === NetworkType.kCellular;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isEthernetNetwork_() {
    return this.network_.type === NetworkType.kEthernet;
  },
});
