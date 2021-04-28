// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './connectivity_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'network-list' is responsible for displaying Ethernet, Cellular,
 *  and WiFi networks.
 */
Polymer({
  is: 'network-list',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {boolean} */
    isTestRunning: {
      type: Boolean,
      value: false,
      notify: true,
    },
  },
});
