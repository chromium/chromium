// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getColorModeString} from './scanning_app_util.js';
import {SelectBehavior} from './select_behavior.js';

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

    /** @type {?string} */
    selectedColorMode: {
      type: String,
      notify: true,
    },
  },

  observers: ['onNumOptionsChange(colorModes.length)'],

  /**
   * @param {number} mojoColorMode
   * @return {!string}
   * @private
   */
  getColorModeString_(mojoColorMode) {
    return getColorModeString(mojoColorMode);
  },
});
