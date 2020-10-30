// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './routine_result_list.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {RoutineName} from './diagnostics_types.js';
import {getSystemRoutineController} from './mojo_interface_provider.js';
import {RoutineListExecutor} from './routine_list_executor.js'

/**
 * @fileoverview
 * 'routine-section' has a button to run tests and displays their results. The
 * parent element eg. a CpuCard binds to the routines property to indicate
 * which routines this instance will run.
 */
Polymer({
  is: 'routine-section',

  _template: html`{__html_template__}`,

  /**
   * @private {?RoutineListExecutor}
   */
  executor_: null,

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
    const resultListElem = this.getResultListElem_();
    resultListElem.initializeTestRun(this.routines);

    this.executor_ = new RoutineListExecutor(getSystemRoutineController());
    this.executor_
        .runRoutines(
            this.routines, resultListElem.onStatusUpdate.bind(resultListElem))
        .then(() => {
          this.isRunTestsDisabled_ = false;
        });
  },

  /**
   * @return {!HTMLElement}
   * @private
   **/
  getResultListElem_() {
    return /** @type {!HTMLElement} */ (this.$$('routine-result-list'));
  },

  /** @override */
  created() {},
});
