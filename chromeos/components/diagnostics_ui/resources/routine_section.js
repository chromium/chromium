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
import {ExecutionProgress, RoutineListExecutor} from './routine_list_executor.js'

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

    /**
     * @type {!ExecutionProgress}
     * @private
     */
    executionStatus_: {
      type: Number,
      value: ExecutionProgress.kNotStarted,
    },

    /** @private */
    isRunTestsDisabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isReportListHidden_: {
      type: Boolean,
      value: true,
    }
  },

  /** @private */
  getResultListElem_() {
    return /** @type {!RoutineResultListElement} */ (
        this.$$('routine-result-list'));
  },

  /** @private */
  onRunTestsClicked_() {
    this.isRunTestsDisabled_ = true;
    const resultListElem = this.getResultListElem_();
    resultListElem.initializeTestRun(this.routines);

    this.executor_ = new RoutineListExecutor(getSystemRoutineController());
    this.executor_
        .runRoutines(
            this.routines,
            (status) => {
              this.executionStatus_ = status.progress;
              resultListElem.onStatusUpdate.call(resultListElem, status);
            })
        .then(() => {
          this.isRunTestsDisabled_ = false;
        });
  },

  /** @private */
  onToggleReportClicked_() {
    // Toggle report list visibility
    this.isReportListHidden_ = !this.isReportListHidden_;
  },

  /** @protected */
  isReportButtonHidden_() {
    return this.executionStatus_ === ExecutionProgress.kNotStarted;
  },

  /** @protected */
  getReportToggleButtonText_() {
    // TODO(joonbug): Localize this string.
    return this.isReportListHidden_ ? 'See Report' : 'Hide Report';
  },

  /** @override */
  created() {},
});
