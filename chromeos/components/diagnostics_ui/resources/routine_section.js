// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './routine_result_list.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {RoutineName} from './diagnostics_types.js';

/**
 * @fileoverview
 * 'routine-section' has a button to run tests and displays their results. The
 * parent element eg. a CpuCard binds to the routines property to indicate
 * which routines this instance will run.
 */
Polymer({
  is: 'routine-section',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!Array<!RoutineName>} */
    routines: {
      type: Array,
      value: () => [],
    },

    /** @private */
    isRunTestsDisabled_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onRunTestsClicked_() {
    this.isRunTestsDisabled_ = true;
    this.getListElem_().initializeTestRun(this.routines);

    // TODO(zentaro): Run tests which will also reenable button on completion.
  },

  /** @return {!HTMLElement} */
  getListElem_() {
    return /** @type {!HTMLElement} */ (this.$$('routine-result-list'));
  },

  /** @override */
  created() {},
});
