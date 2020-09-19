// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './battery_health_card.js';
import './cpu_card.js';
import './diagnostics_shared_css.js';
import './memory_card.js';
import './overview_card.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'diagnostics-app' is the main page for viewing telemetric system information
 * and running diagnostic tests.
 */
Polymer({
  is: 'diagnostics-app',

  _template: html`{__html_template__}`,

  /** @override */
  ready() {
  },

});