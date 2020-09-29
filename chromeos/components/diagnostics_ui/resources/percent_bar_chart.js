// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'percent-bar-chart' is a styling wrapper for paper-progress used to display a
 * percentage based bar chart.
 */
Polymer({
  is: 'percent-bar-chart',

  _template: html`{__html_template__}`,

  properties: {
    title: {
      type: String,
    },

    value: {
      type: Number,
      value: 0,
    },

    max: {
      type: Number,
      value: 100,
    },
  },

  /**
   * Returns the percentage of the current bar chart, rounded to the nearest
   * whole number.
   * @param {number} currentValue
   * @param {number} maxValue
   * @return {number}
   * @private
   */
  computePercentage_(currentValue, maxValue) {
    return Math.round(100 * currentValue / maxValue);
  }
});
