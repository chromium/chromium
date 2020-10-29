// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scan_settings_section.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SelectBehavior} from './select_behavior.js';

/**
 * @fileoverview
 * 'scan-to-select' displays the chosen directory to save completed scans.
 */
Polymer({
  is: 'scan-to-select',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, SelectBehavior],
});
