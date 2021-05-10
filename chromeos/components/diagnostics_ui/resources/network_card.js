// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './network_info.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'network-card' is a styling wrapper for a network-info element.
 */
Polymer({
  is: 'network-card',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {string} */
    guid: {
      type: String,
      value: '',
    },
  },
});
