// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'network-info' is responsible for observing a network guid and
 * displaying a specialized network card (Ethernet, WiFi, Cellular).
 */
Polymer({
  is: 'network-info',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {string} */
    guid: {
      type: String,
      value: '',
    },
  },
});
