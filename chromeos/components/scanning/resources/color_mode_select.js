// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {alphabeticalCompare, getColorModeString} from './scanning_app_util.js';
import {SelectBehavior} from './select_behavior.js';

/** @type {chromeos.scanning.mojom.ColorMode} */
const DEFAULT_COLOR_MODE = chromeos.scanning.mojom.ColorMode.kColor;

/**
 * @fileoverview
 * 'color-mode-select' displays the available scanner color modes in a dropdown.
 */
Polymer({
  is: 'color-mode-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  properties: {
    /** @type {!Array<chromeos.scanning.mojom.ColorMode>} */
    colorModes: {
      type: Array,
      value: () => [],
    },

    /** @type {string} */
    selectedColorMode: {
      type: String,
      notify: true,
    },
  },

  observers: ['onColorModesChange_(colorModes.*)'],

  /**
   * @param {chromeos.scanning.mojom.ColorMode} mojoColorMode
   * @return {string}
   * @private
   */
  getColorModeString_(mojoColorMode) {
    return getColorModeString(mojoColorMode);
  },

  /**
   * Get the index of the default option if it exists. If not, use the index of
   * the first color mode in the color modes array.
   * @return {number}
   * @private
   */
  getDefaultSelectedColorModeIndex_() {
    assert(this.colorModes.length > 0);

    const defaultColorModeIndex = this.colorModes.findIndex((colorMode) => {
      return this.isDefaultColorMode_(colorMode);
    });

    return defaultColorModeIndex === -1 ? 0 : defaultColorModeIndex;
  },

  /**
   * Sorts the color modes and sets the selected color mode when the color modes
   * array changes.
   * @private
   */
  onColorModesChange_() {
    if (this.colorModes.length > 1) {
      this.colorModes = this.customSort(
          this.colorModes, alphabeticalCompare,
          (colorMode) => getColorModeString(colorMode));
    }

    if (this.colorModes.length > 0) {
      const selectedColorModeIndex = this.getDefaultSelectedColorModeIndex_();
      this.selectedColorMode =
          this.colorModes[selectedColorModeIndex].toString();
      this.$.colorModeSelect.selectedIndex = selectedColorModeIndex;
    }
  },

  /**
   * @param {!chromeos.scanning.mojom.ColorMode} colorMode
   * @return {boolean}
   * @private
   */
  isDefaultColorMode_(colorMode) {
    return colorMode === DEFAULT_COLOR_MODE;
  },
});
