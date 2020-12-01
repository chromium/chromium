// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './routine_result_list.js';
import './text_badge.js';
import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RoutineType, StandardRoutineResult, SystemRoutineControllerInterface} from './diagnostics_types.js';
import {getSystemRoutineController} from './mojo_interface_provider.js';
import {ExecutionProgress, RoutineListExecutor} from './routine_list_executor.js';
import {getRoutineType} from './routine_result_entry.js';
import {BadgeType} from './text_badge.js';

/**
 * @fileoverview
 * 'routine-section' has a button to run tests and displays their results. The
 * parent element eg. a CpuCard binds to the routines property to indicate
 * which routines this instance will run.
 */
Polymer({
  is: 'routine-section',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {?RoutineListExecutor}
   */
  executor_: null,

  /**
   * Boolean whether last run had at least one failure,
   * @type {boolean}
   * @private
   */
  hasTestFailure_: false,

  /** @private {?SystemRoutineControllerInterface} */
  systemRoutineController_: null,

  properties: {
    /** @type {!Array<!RoutineType>} */
    routines: {
      type: Array,
      value: () => [],
    },

    /**
     * Overall ExecutionProgress of the routine.
     * @type {!ExecutionProgress}
     * @private
     */
    executionStatus_: {
      type: Number,
      value: ExecutionProgress.kNotStarted,
    },

    /**
     * Name of currently running test
     * @private {string}
     */
    currentTestName_: {
      type: String,
      value: '',
    },

    /** @type {boolean} */
    isTestRunning: {
      type: Boolean,
      notify: true,
    },

    /** @private {boolean} */
    isReportListHidden_: {
      type: Boolean,
      value: true,
    },

    /** @type {string} */
    runTestsButtonText: {
      type: String,
      value: '',
    },
  },

  /** @private */
  getResultListElem_() {
    return /** @type {!RoutineResultListElement} */ (
        this.$$('routine-result-list'));
  },

  /** @private */
  onRunTestsClicked_() {
    this.isTestRunning = true;
    this.hasTestFailure_ = false;

    this.systemRoutineController_ = getSystemRoutineController();
    this.systemRoutineController_.getSupportedRoutines().then((supported) => {
      const filteredRoutines =
          this.routines.filter(routine => supported.routines.includes(routine));

      const resultListElem = this.getResultListElem_();
      resultListElem.initializeTestRun(filteredRoutines);

      this.executor_ =
          new RoutineListExecutor(assert(this.systemRoutineController_));
      this.executionStatus_ = ExecutionProgress.kRunning;
      this.executor_
          .runRoutines(
              filteredRoutines,
              (status) => {
                this.currentTestName_ = loadTimeData.getStringF(
                    'routineNameText', getRoutineType(status.routine));

                if (status.result &&
                    status.result.simpleResult !==
                        chromeos.diagnostics.mojom.StandardRoutineResult
                            .kTestPassed) {
                  this.hasTestFailure_ = true;
                }

                resultListElem.onStatusUpdate.call(resultListElem, status);
              })
          .then(() => {
            this.executionStatus_ = ExecutionProgress.kCompleted;
            this.isTestRunning = false;
            this.cleanUp_();
          });
    });
  },

  /** @private */
  cleanUp_() {
    if (this.executor_) {
      this.executor_.close();
      this.executor_ = null;
    }
    this.systemRoutineController_ = null;
  },

  /** @private */
  onToggleReportClicked_() {
    // Toggle report list visibility
    this.isReportListHidden_ = !this.isReportListHidden_;
  },

  /** @private */
  onLearnMoreClicked_() {
    // TODO(joonbug): Update this link with a p-link
    window.open('https://support.google.com/chromebook/answer/6309225');
  },

  /** @protected */
  isResultAndStatusHidden_() {
    return this.executionStatus_ === ExecutionProgress.kNotStarted;
  },

  /** @protected */
  getReportToggleButtonText_() {
    return loadTimeData.getString(
        this.isReportListHidden_ ? 'seeReportText' : 'hideReportText');
  },

  /** @protected */
  getBadgeType_() {
    if (this.executionStatus_ === ExecutionProgress.kCompleted) {
      if (this.hasTestFailure_) {
        return BadgeType.ERROR;
      }
      return BadgeType.SUCCESS;
    }
    return BadgeType.DEFAULT;
  },

  /** @protected */
  getBadgeText_() {
    if (this.executionStatus_ === ExecutionProgress.kRunning) {
      return loadTimeData.getString('testRunning');
    }
    return loadTimeData.getString(
        this.hasTestFailure_ ? 'testFailedBadgeText' :
                               'testSucceededBadgeText');
  },

  /** @protected */
  getTextStatus_() {
    if (this.executionStatus_ === ExecutionProgress.kRunning) {
      return this.currentTestName_;
    }
    return loadTimeData.getString(
        this.hasTestFailure_ ? 'testFailure' : 'testSuccess');
  },

  /** @override */
  detached() {
    this.cleanUp_();
  },

  /** @override */
  created() {},
});
