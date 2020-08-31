// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    // TODO(jimmyxgong): Remove this once the app has more capabilities.
    this.$$('#header').textContent = 'Diagnostics';
  },

});