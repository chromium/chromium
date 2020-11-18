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

/** @type {number} */
const DEFAULT_RESOLUTION = 300;

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

    /** @type {string} */
    selectedResolution: {
      type: String,
      notify: true,
    },
  },

  observers: ['onResolutionsChange_(resolutions.*)'],

  /**
   * @param {number} resolution
   * @return {string}
   * @private
   */
  getResolutionString_(resolution) {
    return loadTimeData.getStringF(
        'resolutionOptionText', resolution.toString());
  },

  /**
   * 300 dpi should be the default option if it exists. If not, use the first
   * resolution in the resolutions array.
   * @return {string}
   * @private
   */
  getDefaultSelectedResolution_() {
    const defaultResolutionIndex = this.resolutions.findIndex((resolution) => {
      return this.isDefaultResolution_(resolution);
    });

    return defaultResolutionIndex === -1 ?
        this.resolutions[0].toString() :
        this.resolutions[defaultResolutionIndex].toString();
  },

  /**
   * Sorts the resolutions and sets the selected resolution when the resolutions
   * array changes.
   * @private
   */
  onResolutionsChange_() {
    if (this.resolutions.length > 1) {
      // Sort the resolutions in descending order.
      this.resolutions.sort(function(a, b) {
        return b - a;
      });
    }

    if (this.resolutions.length > 0) {
      this.selectedResolution = this.getDefaultSelectedResolution_();
    }
  },

  /**
   * @param {number} resolution
   * @return {boolean}
   * @private
   */
  isDefaultResolution_(resolution) {
    return resolution === DEFAULT_RESOLUTION;
  },
});
