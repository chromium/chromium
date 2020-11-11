// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_fonts_css.js';
import './scanning_shared_css.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppState} from './scanning_app_types.js';

/**
 * @fileoverview
 * 'scan-preview' shows a preview of a scanned document.
 */
Polymer({
  is: 'scan-preview',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!AppState} */
    appState: Number,

    /** @type {number} */
    pageNumber: Number,

    /** @type {number} */
    progressPercent: Number,
  },

  /**
   * Returns the translated helper text string with the id attribute. The id is
   * used to selectively style parts of the string.
   * @return {string}
   * @private
   */
  getHelperTextHtml_() {
    return this.i18nAdvanced('scanPreviewHelperText', {attrs: ['id']});
  },

  /**
   * @return {string}
   * @private
   */
  getProgressTextString_() {
    return loadTimeData.getStringF(
        'scanPreviewProgressText', this.pageNumber.toString());
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldHideHelperText_() {
    return this.appState === AppState.SCANNING ||
        this.appState === AppState.DONE;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldHideProgress_() {
    return this.appState !== AppState.SCANNING;
  },
});
