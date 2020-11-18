// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';
import './throbber_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ScannerArr} from './scanning_app_types.js';
import {alphabeticalCompare, getScannerDisplayName, tokenToString} from './scanning_app_util.js';
import {SelectBehavior} from './select_behavior.js';

/**
 * @fileoverview
 * 'scanner-select' displays the connected scanners in a dropdown.
 */
Polymer({
  is: 'scanner-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

  properties: {
    /** @type {!ScannerArr} */
    scanners: {
      type: Array,
      value: () => [],
    },

    /** @type {string} */
    selectedScannerId: {
      type: String,
      notify: true,
    },

    loaded: Boolean,
  },

  observers: ['onScannersChange_(scanners.*)'],

  /**
   * @param {!chromeos.scanning.mojom.Scanner} scanner
   * @return {string}
   * @private
   */
  getScannerDisplayName_(scanner) {
    return getScannerDisplayName(scanner);
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
   * Sorts the scanners and sets the selected scanner when the scanners array
   * changes.
   * @private
   */
  onScannersChange_() {
    if (this.scanners.length > 1) {
      this.scanners = this.customSort(
          this.scanners, alphabeticalCompare,
          (scanner) => getScannerDisplayName(scanner));
    }

    // If it exists, select the first option in the sorted array as the default.
    if (this.scanners.length > 0) {
      this.selectedScannerId = tokenToString(this.scanners[0].id);
    }
  },

  /**
   * Helper function to decide whether to hide the scanner connection help link.
   * @protected
   */
  shouldHideScanHelpLink_() {
    return !this.loaded || this.scanners.length !== 0;
  }
});
