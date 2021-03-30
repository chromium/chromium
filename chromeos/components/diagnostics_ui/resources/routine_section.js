// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './diagnostics_card.js';
import './diagnostics_shared_css.js';
import './icons.js';
import './routine_result_list.js';
import './text_badge.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PowerRoutineResult, RoutineType, StandardRoutineResult, SystemRoutineControllerInterface} from './diagnostics_types.js';
import {getSystemRoutineController} from './mojo_interface_provider.js';
import {ExecutionProgress, RoutineListExecutor} from './routine_list_executor.js';
import {getRoutineType, getSimpleResult} from './routine_result_entry.js';
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
     * Total time in minutes of estimate runtime based on routines array.
     * @type {number}
     */
    routineRuntime: {
      type: Number,
      value: 0,
    },

    /**
     * Timestamp of when routine test started execution in milliseconds.
     * @private {number}
     */
    routineStartTimeMs_: {
      type: Number,
      value: -1,
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

    /** @type {boolean} */
    isPowerRoutine: {
      type: Boolean,
      value: false,
    },

    /** @private {?PowerRoutineResult} */
    powerRoutineResult_: {
      type: Object,
      value: null,
    },

    /** @type {string} */
    runTestsButtonText: {
      type: String,
      value: '',
    },

    /** @type {string} */
    additionalMessage: {
      type: String,
      value: '',
    },

    /** @type {string} */
    learnMoreLinkSection: {
      type: String,
      value: '',
    },

    /** @private {!BadgeType} */
    badgeType_: {
      type: String,
      value: BadgeType.RUNNING,
    },

    /** @private {string} */
    badgeText_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    statusText_: {
      type: String,
      value: '',
    },

    /** @private {boolean} */
    isLoggedIn_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isLoggedIn'),
    },

    /** @type {boolean} */
    shouldShowCautionBanner: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'routineStatusChanged_(executionStatus_, currentTestName_,' +
        'additionalMessage)',
  ],

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
    const resultListElem = this.getResultListElem_();
    this.systemRoutineController_.getSupportedRoutines().then((supported) => {
      const filteredRoutines =
          this.routines.filter(routine => supported.routines.includes(routine));

      resultListElem.initializeTestRun(filteredRoutines);

      // Expand result list by default.
      if (!this.shouldHideReportList_()) {
        this.$.collapse.show();
      }

      if (this.shouldShowCautionBanner) {
        this.showCautionBanner_(loadTimeData.getString('cpuBannerMessage'));
      }

      this.routineStartTimeMs_ = performance.now();
      const remainingTimeUpdaterId =
          setInterval(() => this.setRunningStatusBadgeText_(), 1000);

      this.executor_ =
          new RoutineListExecutor(assert(this.systemRoutineController_));
      this.executor_
          .runRoutines(
              filteredRoutines,
              (status) => {
                if (status.result && status.result.powerResult) {
                  this.powerRoutineResult_ = status.result.powerResult;
                }

                if (status.result &&
                    getSimpleResult(status.result) !==
                        chromeos.diagnostics.mojom.StandardRoutineResult
                            .kTestPassed) {
                  this.hasTestFailure_ = true;
                }

                // Execution progress is checked here to avoid overwriting the
                // test name shown in the status text.
                if (status.progress !== ExecutionProgress.kCancelled) {
                  this.currentTestName_ = getRoutineType(status.routine);
                }

                this.executionStatus_ = status.progress;

                resultListElem.onStatusUpdate.call(resultListElem, status);
              })
          .then((/** @type {!ExecutionProgress} */ status) => {
            this.executionStatus_ = status;
            this.isTestRunning = false;
            this.routineStartTimeMs_ = -1;
            this.runTestsButtonText =
                loadTimeData.getString('runAgainButtonText');
            clearInterval(remainingTimeUpdaterId);
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

    if (this.shouldShowCautionBanner) {
      this.dismissCautionBanner_();
    }

    this.systemRoutineController_ = null;
  },

  /** @protected */
  stopTests_() {
    if (this.executor_) {
      this.executor_.cancel();
    }
  },

  /** @private */
  onToggleReportClicked_() {
    // Toggle report list visibility
    this.$.collapse.toggle();
  },

  /** @protected */
  onLearnMoreClicked_() {
    const baseSupportUrl =
        'https://support.google.com/chromebook?p=diagnostics_';
    assert(this.learnMoreLinkSection);

    window.open(baseSupportUrl + this.learnMoreLinkSection);
  },

  /** @protected */
  isResultButtonHidden_() {
    return this.shouldHideReportList_() ||
        this.executionStatus_ === ExecutionProgress.kNotStarted;
  },

  /** @protected */
  isLearnMoreHidden_() {
    return !this.shouldHideReportList_() || !this.isLoggedIn_ ||
        this.executionStatus_ !== ExecutionProgress.kCompleted;
  },

  /** @protected */
  isStatusHidden_() {
    return this.executionStatus_ === ExecutionProgress.kNotStarted;
  },

  /**
   * @param {boolean} opened Whether the section is expanded or not.
   * @return {string} button text.
   * @protected
   */
  getReportToggleButtonText_(opened) {
    return loadTimeData.getString(opened ? 'hideReportText' : 'seeReportText');
  },

  /**
   * Sets status texts for remaining runtime while the routine runs.
   * @private
   */
  setRunningStatusBadgeText_() {
    // Routines that are longer than 5 minutes are considered large
    const largeRoutine = this.routineRuntime >= 5;

    // Calculate time elapsed since the start of routine in minutes.
    const minsElapsed =
        (performance.now() - this.routineStartTimeMs_) / 1000 / 60;
    let timeRemainingInMin = Math.ceil(this.routineRuntime - minsElapsed);

    if (largeRoutine && timeRemainingInMin <= 0) {
      this.statusText_ =
          loadTimeData.getString('routineRemainingMinFinalLarge');
      return;
    }

    // For large routines, round up to 5 minutes increments.
    if (largeRoutine && timeRemainingInMin % 5 !== 0) {
      timeRemainingInMin += (5 - timeRemainingInMin % 5);
    }

    this.badgeText_ = timeRemainingInMin <= 1 ?
        loadTimeData.getString('routineRemainingMinFinal') :
        loadTimeData.getStringF('routineRemainingMin', timeRemainingInMin);
  },

  /** @protected */
  routineStatusChanged_() {
    switch (this.executionStatus_) {
      case ExecutionProgress.kNotStarted:
        // Do nothing since status is hidden when tests have not been started.
        break;
      case ExecutionProgress.kRunning:
        this.setBadgeAndStatusText_(
            BadgeType.RUNNING, loadTimeData.getString('testRunning'),
            loadTimeData.getStringF(
                'routineNameText', this.currentTestName_.toLowerCase()));
        break;
      case ExecutionProgress.kCancelled:
        this.setBadgeAndStatusText_(
            BadgeType.STOPPED, loadTimeData.getString('testStoppedBadgeText'),
            loadTimeData.getStringF(
                'testCancelledText', this.currentTestName_));
        break;
      case ExecutionProgress.kCompleted:
        const isPowerRoutine = this.isPowerRoutine || this.powerRoutineResult_;
        if (this.hasTestFailure_) {
          this.setBadgeAndStatusText_(
              BadgeType.ERROR, loadTimeData.getString('testFailedBadgeText'),
              loadTimeData.getString('testFailure'));
        } else {
          this.setBadgeAndStatusText_(
              BadgeType.SUCCESS,
              loadTimeData.getString('testSucceededBadgeText'),
              isPowerRoutine ? this.getPowerRoutineString_() :
                               loadTimeData.getString('testSuccess'));
        }
        break;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {string}
   */
  getPowerRoutineString_() {
    const stringId =
        this.routines.includes(
            chromeos.diagnostics.mojom.RoutineType.kBatteryCharge) ?
        'chargeTestResultText' :
        'dischargeTestResultText';
    const percentText = loadTimeData.getStringF(
        'percentageLabel', this.powerRoutineResult_.percentChange.toFixed(2));
    return loadTimeData.getStringF(
        stringId, percentText, this.powerRoutineResult_.timeElapsedSeconds);
  },

  /**
   * @param {!BadgeType} badgeType
   * @param {string} badgeText
   * @param {string} statusText
   * @private
   */
  setBadgeAndStatusText_(badgeType, badgeText, statusText) {
    this.setProperties({
      badgeType_: badgeType,
      badgeText_: badgeText,
      statusText_: statusText
    });
  },

  /**
   * @protected
   * @return {boolean}
   */
  isRunTestsButtonHidden_() {
    return this.isTestRunning &&
        this.executionStatus_ === ExecutionProgress.kRunning;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isStopTestsButtonHidden_() {
    return this.executionStatus_ !== ExecutionProgress.kRunning;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isRunTestsButtonDisabled_() {
    return this.isTestRunning || this.additionalMessage != '';
  },

  /**
   * @protected
   * @return {boolean}
   */
  shouldHideReportList_() {
    return this.routines.length < 2;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isAdditionalMessageHidden_() {
    return this.additionalMessage == '';
  },

  /**
   * @private
   * @param {string} message
   */
  showCautionBanner_(message) {
    this.dispatchEvent(new CustomEvent(
        'show-caution-banner',
        {bubbles: true, composed: true, detail: {message}}));
  },

  /** @private */
  dismissCautionBanner_() {
    this.dispatchEvent(new CustomEvent(
        'dismiss-caution-banner', {bubbles: true, composed: true}));
  },

  /** @override */
  detached() {
    this.cleanUp_();
  },

  /** @override */
  created() {},
});
