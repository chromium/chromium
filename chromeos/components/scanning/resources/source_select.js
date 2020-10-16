// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getSourceTypeString} from './scanning_app_util.js';
import {SelectBehavior} from './select_behavior.js';

/**
 * @fileoverview
 * 'source-select' displays the available scanner sources in a dropdown.
 */
Polymer({
  is: 'source-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],

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
  },

  observers: ['onNumOptionsChange(sources.length)'],

  /**
   * @param {number} mojoSourceType
   * @return {!string}
   * @private
   */
  getSourceTypeString_(mojoSourceType) {
    return getSourceTypeString(mojoSourceType);
  },
});
