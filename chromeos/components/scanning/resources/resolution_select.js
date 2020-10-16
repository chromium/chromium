// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SelectBehavior} from './select_behavior.js';

/**
 * @fileoverview
 * 'resolution-select' displays the available scan resolutions in a dropdown.
 */
Polymer({
  is: 'resolution-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  properties: {
    /** @type {!Array<number>} */
    resolutions: {
      type: Array,
      value: () => [],
    },

    /** @type {?string} */
    selectedResolution: {
      type: String,
      notify: true,
    },
  },

  observers: ['onNumOptionsChange(resolutions.length)'],

  /**
   * @param {number} resolution
   * @return {!string}
   * @private
   */
  getResolutionString_(resolution) {
    return loadTimeData.getStringF(
        'resolutionOptionText', resolution.toString());
  },
});
