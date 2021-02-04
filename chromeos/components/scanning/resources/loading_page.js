// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'loading-page' is shown while searching for available scanners.
 */
Polymer({
  is: 'loading-page',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],
});
