// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
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
    /** @type {string} */
    header: {
      type: String,
    },

    /** @type {number} */
    value: {
      type: Number,
      value: 0,
    },

    /** @type {number} */
    max: {
      type: Number,
      value: 100,
    },
  },

  /**
   * Get adjusted value clamped to max value. paper-progress breaks for a while
   * when value is set higher than max in certain cases (e.g. due to fetching of
   * max being resolved later).
   * @protected
   */
  getAdjustedValue_() {
    return this.value <= this.max ? this.value : this.max;
  }
});
