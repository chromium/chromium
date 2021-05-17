// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './network_info.js';
import './routine_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RoutineType} from './diagnostics_types.js';

/**
 * @fileoverview
 * 'connectivity-card' runs network routines and displays network health data.
 */
Polymer({
  is: 'connectivity-card',

  _template: html`{__html_template__}`,

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
        RoutineType.kCaptivePortal,
        RoutineType.kDnsLatency,
        RoutineType.kDnsResolution,
        RoutineType.kDnsResolverPresent,
        RoutineType.kGatewayCanBePinged,
        RoutineType.kHasSecureWiFiConnection,
        RoutineType.kHttpFirewall,
        RoutineType.kHttpsFirewall,
        RoutineType.kHttpsLatency,
        RoutineType.kLanConnectivity,
        RoutineType.kSignalStrength,
      ],
    },

    /** @type {string} */
    activeGuid: {
      type: String,
      value: '',
    },
  },

  /** @protected */
  getEstimateRuntimeInMinutes_() {
    // Connectivity routines will always last <= 1 minute.
    return 1;
  },
});
