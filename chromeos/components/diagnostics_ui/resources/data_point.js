// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';
import './icons.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'data-point' shows a single piece of information related to a component. It
 *  consists of a header, value, and tooltip that provides context about the
 *  item.
 */
Polymer({
  is: 'data-point',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {string} */
    header: {
      type: String,
    },

    /** @type {string} */
    value: {
      type: String,
      value: '',
    },

    /** @type {string} */
    tooltipText: {
      type: String,
      value: '',
    },

    /** @type {boolean} */
    warningState: {
      type: Boolean,
      value: false,
    },

    /**
     * The alignment of the data point on the screen (vertical or horizontal).
     *  @type {string}
     */
    orientation: {
      type: String,
      value: 'vertical',
      reflectToAttribute: true,
    },
  },

  /** @protected */
  getValueClass_() {
    return this.warningState ? 'value text-red' : 'value';
  },
});
