// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './scanning.mojom-lite.js';
import './throbber_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {ScannerArr} from './scanning_app_types.js';
import {tokenToString} from './scanning_app_util.js';
import './strings.js';

/** @type {number} */
const NUM_REQUIRED_SCANNERS = 2;

/**
 * @fileoverview
 * 'scanner-select' displays the connected scanners in a dropdown.
 */
Polymer({
  is: 'scanner-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!ScannerArr} */
    scanners: {
      type: Array,
      value: () => [],
    },

    /** @type {?string} */
    selectedScannerId: {
      type: String,
      notify: true,
    },

    loaded: Boolean,

    settingsDisabled: Boolean,

    /** @private */
    disabled_: Boolean,
  },

  observers: [
    'updateDisabled_(scanners.length, settingsDisabled)',
  ],

  /**
   * @param {!chromeos.scanning.mojom.Scanner} scanner
   * @return {string}
   * @private
   */
  getScannerDisplayName_(scanner) {
    return scanner.displayName.data.map(ch => String.fromCodePoint(ch))
        .join('');
  },

  /**
   * Converts an unguessable token to a string so it can be used as the value of
   * an option.
   * @param {!chromeos.scanning.mojom.Scanner} scanner
   * @return {string}
   * @private
   */
  getTokenAsString_(scanner) {
    return tokenToString(scanner.id);
  },

  /**
   * Disables the dropdown if settings are disabled or the number of available
   * scanners is less than the number of required scanners.
   * @private
   */
  updateDisabled_() {
    this.disabled_ =
        this.settingsDisabled || this.scanners.length < NUM_REQUIRED_SCANNERS;
  },
});
