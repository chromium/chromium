// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppState} from './scanning_app_types.js';

/**
 * @fileoverview
 * 'scan-done-section' shows the post-scan user options.
 */
Polymer({
  is: 'scan-done-section',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {number} */
    pageNumber: Number,
  },

  /**
   * @return {string}
   * @private
   */
  getTitleText_() {
    return this.i18n(
        this.pageNumber > 1 ? 'fileSavedTextPlural' : 'fileSavedText');
  },

  /** @private */
  onDoneClick_() {
    this.fire('done-click');
  },
});
