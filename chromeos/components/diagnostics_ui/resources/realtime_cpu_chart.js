// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'realtime-cpu-chart' is a moving line graph component used to display a
 * realtime cpu usage information.
 */
Polymer({
  is: 'realtime-cpu-chart',

  _template: html`{__html_template__}`,

  properties: {
    user: {
      type: Number,
      value: 0,
    },

    system: {
      type: Number,
      value: 0,
    },
  },
});
