// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './routine_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Network, NetworkHealthProviderInterface, RoutineType} from './diagnostics_types.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';

/**
 * @fileoverview
 * 'connectivity-card' displays runs network routines and displays
 *  network health data.
 */
Polymer({
  is: 'connectivity-card',

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

    /** @private {!Array<!RoutineType>} */
    routines_: {
      type: Array,
      value: [
        chromeos.diagnostics.mojom.RoutineType.kCaptivePortal,
        chromeos.diagnostics.mojom.RoutineType.kDnsLatency,
        chromeos.diagnostics.mojom.RoutineType.kDnsResolution,
        chromeos.diagnostics.mojom.RoutineType.kDnsResolverPresent,
        chromeos.diagnostics.mojom.RoutineType.kGatewayCanBePinged,
        chromeos.diagnostics.mojom.RoutineType.kHasSecureWiFiConnection,
        chromeos.diagnostics.mojom.RoutineType.kHttpFirewall,
        chromeos.diagnostics.mojom.RoutineType.kHttpsFirewall,
        chromeos.diagnostics.mojom.RoutineType.kHttpsLatency,
        chromeos.diagnostics.mojom.RoutineType.kLanConnectivity,
        chromeos.diagnostics.mojom.RoutineType.kSignalStrength,
      ],
    },

    /** @type {string} */
    activeGuid: {
      type: String,
      value: '',
    },

    /** @private {!Network} */
    network_: {
      type: Object,
    },
  },

  observers: ['observeNetwork_(activeGuid)'],

  /** @override */
  created() {
    this.networkHealthProvider_ = getNetworkHealthProvider();
  },

  /** @protected */
  getEstimateRuntimeInMinutes_() {
    // Connectivity routines will always last <= 1 minute.
    return 1;
  },

  /** @private */
  observeNetwork_() {
    if (!this.activeGuid) {
      return;
    }
    // Calling observeNetwork will trigger onNetworkStateChanged.
    this.networkHealthProvider_.observeNetwork(this, this.activeGuid);
  },

  /**
   * Implements NetworkStateObserver.onNetworkStateChanged
   * @param {!Network} network
   */
  onNetworkStateChanged(network) {
    this.network_ = network;
  },

  /** @override */
  detached() {
    // TODO(michaelcheco): Stop observing activeGuid when the real
    // observeNetwork implementation is added.
  },
});