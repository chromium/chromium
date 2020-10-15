// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getSourceTypeString} from './scanning_app_util.js';

/** @type {number} */
const NUM_REQUIRED_SOURCES = 2;

/**
 * @fileoverview
 * 'source-select' displays the available scanner sources in a dropdown.
 */
Polymer({
  is: 'source-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!Array<!chromeos.scanning.mojom.ScanSource>} */
    sources: {
      type: Array,
      value: () => [],
    },

    /** @type {?string} */
    selectedSource: {
      type: String,
      notify: true,
    },

    settingsDisabled: Boolean,

    /** @private */
    disabled_: Boolean,
  },

  observers: [
    'updateDisabled_(sources.length, settingsDisabled)',
  ],

  /**
   * @param {number} mojoSourceType
   * @return {!string}
   * @private
   */
  getSourceTypeString_(mojoSourceType) {
    return getSourceTypeString(mojoSourceType);
  },

  /**
   * Disables the dropdown if settings are disabled or the number of available
   * sources is less than the number of required sources.
   * @private
   */
  updateDisabled_() {
    this.disabled_ =
        this.settingsDisabled || this.sources.length < NUM_REQUIRED_SOURCES;
  },
});
