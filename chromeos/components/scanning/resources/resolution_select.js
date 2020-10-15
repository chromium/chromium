// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import './strings.js';

/** @type {number} */
const NUM_REQUIRED_RESOLUTIONS = 2;

/**
 * @fileoverview
 * 'resolution-select' displays the available scan resolutions in a dropdown.
 */
Polymer({
  is: 'resolution-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!Array<number>} */
    resolutions: {
      type: Array,
      value: () => [],
    },

    /** @type {number|undefined} */
    selectedResolution: {
      type: Number,
      notify: true,
    },

    /**
     * Indicates whether all settings have been disabled by the parent element.
     */
    settingsDisabled: Boolean,

    /**
     * Controls whether the dropdown is disabled.
     * @private
     */
    disabled_: {
      type: Boolean,
      computed: 'computeDisabled_(resolutions.length, settingsDisabled)',
    },
  },

  /**
   * @param {number} resolution
   * @return {!string}
   * @private
   */
  getResolutionString_(resolution) {
    return loadTimeData.getStringF(
        'resolutionOptionText', resolution.toString());
  },

  /**
   * Disables the dropdown if settings are disabled or the number of available
   * resolutions is less than the number of required resolutions.
   * @return {boolean}
   * @private
   */
  computeDisabled_() {
    return this.settingsDisabled ||
        this.resolutions.length < NUM_REQUIRED_RESOLUTIONS;
  },
});
