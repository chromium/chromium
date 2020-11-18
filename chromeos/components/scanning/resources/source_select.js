// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning.mojom-lite.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {alphabeticalCompare, getSourceTypeString} from './scanning_app_util.js';
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

    /** @type {string} */
    selectedSource: {
      type: String,
      notify: true,
    },
  },

  observers: ['onSourcesChange_(sources.*)'],

  /**
   * @param {chromeos.scanning.mojom.SourceType} mojoSourceType
   * @return {string}
   * @private
   */
  getSourceTypeString_(mojoSourceType) {
    return getSourceTypeString(mojoSourceType);
  },

  /**
   * "Flatbed" should always be the default option if it exists. If not, use
   * the first source in the sources array.
   * @return {string}
   * @private
   */
  getDefaultSelectedSource_() {
    const flatbedSourceIndex = this.sources.findIndex((source) => {
      return source.type === chromeos.scanning.mojom.SourceType.kFlatbed;
    });

    return flatbedSourceIndex === -1 ? this.sources[0].name :
                                       this.sources[flatbedSourceIndex].name;
  },

  /**
   * Sorts the sources and sets the selected source when sources change.
   * @private
   */
  onSourcesChange_() {
    if (this.sources.length > 1) {
      this.sources = this.customSort(
          this.sources, alphabeticalCompare,
          (source) => getSourceTypeString(source.type));
    }

    if (this.sources.length > 0) {
      this.selectedSource = this.getDefaultSelectedSource_();
    }
  },

  /**
   * @param {!chromeos.scanning.mojom.SourceType} sourceType
   * @return {boolean}
   * @private
   */
  isDefaultSource_(sourceType) {
    return sourceType === chromeos.scanning.mojom.SourceType.kFlatbed;
  },
});
