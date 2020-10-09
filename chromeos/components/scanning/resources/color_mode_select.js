// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';

import {getColorModeString} from './scanning_app_util.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import './strings.js';

/** @type {number} */
const NUM_REQUIRED_COLOR_MODES = 2;

/**
 * @fileoverview
 * 'color-mode-select' displays the available scanner color modes in a dropdown.
 */
Polymer({
  is: 'color-mode-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!Array<chromeos.scanning.mojom.ColorMode>} */
    colorModes: {
      type: Array,
      value: () => [],
    },

    /** @type {chromeos.scanning.mojom.ColorMode|undefined} */
    selectedColorMode: {
      type: chromeos.scanning.mojom.ColorMode,
      notify: true,
    },

    settingsDisabled: Boolean,

    /** @private */
    disabled_: Boolean,
  },

  observers: [
    'updateDisabled_(colorModes.length, settingsDisabled)',
  ],

  /**
   * @param {number} mojoColorMode
   * @return {!string}
   * @private
   */
  getColorModeString_(mojoColorMode) {
    return getColorModeString(mojoColorMode);
  },

  /**
   * Disables the dropdown if settings are disabled or the number of available
   * color modes is less than the number of required color modes.
   * @private
   */
  updateDisabled_() {
    this.disabled_ = this.settingsDisabled ||
        this.colorModes.length < NUM_REQUIRED_COLOR_MODES;
  },
});
