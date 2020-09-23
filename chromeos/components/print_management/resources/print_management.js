// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import './print_job_clear_history_dialog.js';
import './print_job_entry.js';
import './print_management_fonts_css.js';
import './print_management_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getMetadataProvider} from './mojo_interface_provider.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';

const METADATA_STORED_INDEFINITELY = -1;
const METADATA_STORED_FOR_ONE_DAY = 1;
const METADATA_NOT_STORED = 0;

/**
 * @typedef {Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
 */
let PrintJobInfoArr;

/**
 * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} first
 * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} second
 * @return {number}
 */
function comparePrintJobsReverseChronologically(first, second) {
  return -comparePrintJobsChronologically(first, second);
}

/**
 * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} first
 * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} second
 * @return {number}
 */
function comparePrintJobsChronologically(first, second) {
  return Number(first.creationTime.internalValue) -
      Number(second.creationTime.internalValue);
}

/**
 * @fileoverview
 * 'print-management' is used as the main app to display print jobs.
 */
Polymer({
  is: 'print-management',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  /**
   * @private {
   *  ?chromeos.printing.printingManager.mojom.PrintingMetadataProviderInterface
   * }
   */
  mojoInterfaceProvider_: null,

  /**
   * Receiver responsible for observing print job updates notification events.
   * @private {
   *  ?chromeos.printing.printingManager.mojom.PrintJobsObserverReceiver}
   */
  printJobsObserverReceiver_: null,

  properties: {
    /**
     * @type {!PrintJobInfoArr}
     * @private
     */
    printJobs_: {
      type: Array,
      value: () => [],
    },

    /** @private */
    printJobHistoryExpirationPeriod_: {
      type: String,
      value: '',
    },

    /** @private */
    activeHistoryInfoIcon_: {
      type: String,
      value: '',
    },

    /** @private */
    isPolicyControlled_: {
      type: Boolean,
      value: false,
    },

    /**
     * @type {!PrintJobInfoArr}
     * @private
     */
    ongoingPrintJobs_: {
      type: Array,
      value: () => [],
    },

    /**
     * Used by FocusRowBehavior to track the last focused element on a row.
     * @private
     */
    lastFocused_: Object,

    /**
     * Used by FocusRowBehavior to track if the list has been blurred.
     * @private
     */
    listBlurred_: Boolean,

    /** @private */
    showClearAllButton_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** @private */
    showClearAllDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    deletePrintJobHistoryAllowedByPolicy_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    shouldDisableClearAllButton_: {
      type: Boolean,
      computed: 'computeShouldDisableClearAllButton_(printJobs_,' +
          'deletePrintJobHistoryAllowedByPolicy_)',
    }
  },

  listeners: {
    'all-history-cleared': 'getPrintJobs_',
    'remove-print-job' : 'removePrintJob_',
  },

  observers: ['onClearAllButtonUpdated_(shouldDisableClearAllButton_)'],

  /** @override */
  created() {
    this.mojoInterfaceProvider_ = getMetadataProvider();

    window.CrPolicyStrings = {
      controlledSettingPolicy:
          loadTimeData.getString('clearAllPrintJobPolicyIndicatorToolTip'),
    };
  },

  /** @override */
  attached() {
    this.getPrintJobHistoryExpirationPeriod_();
    this.startObservingPrintJobs_();
    this.fetchDeletePrintJobHistoryPolicy_();
  },

  /** @override */
  detached() {
    this.printJobsObserverReceiver_.$.close();
  },

  /** @private */
  startObservingPrintJobs_() {
    this.printJobsObserverReceiver_ =
        new chromeos.printing.printingManager.mojom.PrintJobsObserverReceiver
        (
          /**
           * @type {!chromeos.printing.printingManager.mojom.
           *        PrintJobsObserverInterface}
           */
          (this));
    this.mojoInterfaceProvider_.observePrintJobs(
        this.printJobsObserverReceiver_.$.bindNewPipeAndPassRemote())
        .then(() => {
          this.getPrintJobs_();
        });
  },

  /** @private */
  fetchDeletePrintJobHistoryPolicy_() {
    this.mojoInterfaceProvider_.getDeletePrintJobHistoryAllowedByPolicy()
        .then(/*@type {!{isAllowedByPolicy: boolean}}*/(param) => {
            this.onGetDeletePrintHistoryPolicy_(param)});
  },

  /**
   * @param {!{isAllowedByPolicy: boolean}} responseParam
   * @private
   */
  onGetDeletePrintHistoryPolicy_(responseParam) {
    this.showClearAllButton_ = true;
    this.deletePrintJobHistoryAllowedByPolicy_ =
        responseParam.isAllowedByPolicy;
  },

  /**
   * Overrides chromeos.printing.printingManager.mojom.
   *           PrintJobsObserverInterface
   */
  onAllPrintJobsDeleted() {
    this.getPrintJobs_();
  },

  /**
   * Overrides chromeos.printing.printingManager.mojom.
   *           PrintJobsObserverInterface
   * @param {!chromeos.printing.printingManager.mojom.PrintJobInfo} job
   */
  onPrintJobUpdate(job) {
    // Only update ongoing print jobs.
    assert(job.activePrintJobInfo);

    // Check if |job| is an existing ongoing print job and requires an update
    // or if |job| is a new ongoing print job.
    const idx = this.getIndexOfOngoingPrintJob_(job.id);
    if (idx !== -1) {
      // Replace the existing ongoing print job with its updated entry.
      this.splice('ongoingPrintJobs_', idx, 1, job);
    } else {
      // New ongoing print jobs are appended to the ongoing print
      // jobs list.
      this.push('ongoingPrintJobs_', job);
    }

    if (job.activePrintJobInfo.activeState ===
        chromeos.printing.printingManager.mojom.ActivePrintJobState
            .kDocumentDone) {
      // This print job is now completed, next step is to update the history
      // list with the recently stored print job.
      this.getPrintJobs_();
    }
  },

  /**
   * @param {!{printJobs: !PrintJobInfoArr}} jobs
   * @private
   */
  onPrintJobsReceived_(jobs) {
    // TODO(crbug/1073690): Update this when BigInt is supported for
    // updateList().
    let ongoingList = [];
    let historyList = [];
    for (const job of jobs.printJobs) {
      // activePrintJobInfo is not null for ongoing print jobs.
      if (job.activePrintJobInfo) {
        ongoingList.push(job);
      }
      else {
        historyList.push(job);
      }
    }

    // Sort the print jobs in chronological order.
    this.ongoingPrintJobs_ =
        ongoingList.sort(comparePrintJobsChronologically);
    this.printJobs_ = historyList.sort(comparePrintJobsReverseChronologically);
  },

  /** @private */
  getPrintJobs_() {
    this.mojoInterfaceProvider_.getPrintJobs()
      .then(this.onPrintJobsReceived_.bind(this));
  },

  /**
   * @param {!{
   *     expirationPeriodInDays: number,
   *     isFromPolicy: boolean
   * }}  printJobPolicyInfo
   * @private
   */
  onPrintJobHistoryExpirationPeriodReceived_(printJobPolicyInfo) {
    const expirationPeriod = printJobPolicyInfo.expirationPeriodInDays;
    // If print jobs are not persisted, we can return early since the tooltip
    // section won't be shown.
    if (expirationPeriod === METADATA_NOT_STORED) {
      return;
    }

    this.isPolicyControlled_ = printJobPolicyInfo.isFromPolicy;
    this.activeHistoryInfoIcon_ = this.isPolicyControlled_
      ? 'enterpriseIcon'
      : 'infoIcon';

    switch (expirationPeriod) {
      case METADATA_STORED_INDEFINITELY:
        this.printJobHistoryExpirationPeriod_ =
          loadTimeData.getString('printJobHistoryIndefinitePeriod');
        break;
      case METADATA_STORED_FOR_ONE_DAY:
        this.printJobHistoryExpirationPeriod_ =
          loadTimeData.getString('printJobHistorySingleDay');
        break;
      default:
        this.printJobHistoryExpirationPeriod_ =
          loadTimeData.getStringF(
          'printJobHistoryExpirationPeriod',
          expirationPeriod
        );
    }
  },

  /** @private */
  getPrintJobHistoryExpirationPeriod_() {
    this.mojoInterfaceProvider_.getPrintJobHistoryExpirationPeriod()
      .then(this.onPrintJobHistoryExpirationPeriodReceived_.bind(this));
  },

  /**
   * @param {!CustomEvent<string>} e
   * @private
   */
  removePrintJob_(e) {
    const idx = this.getIndexOfOngoingPrintJob_(e.detail) ;
    if (idx !== -1) {
      this.splice('ongoingPrintJobs_', idx, 1);
    }
  },

  /** @private */
  onClearHistoryClicked_() {
    this.showClearAllDialog_ = true;
  },

  /** @private */
  onClearHistoryDialogClosed_() {
    this.showClearAllDialog_ = false;
  },

  /**
   * @param {string} expectedId
   * @return {number}
   * @private
   */
  getIndexOfOngoingPrintJob_(expectedId) {
    return this.ongoingPrintJobs_.findIndex(
        arr_job => arr_job.id === expectedId
    );
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldDisableClearAllButton_() {
    return !this.deletePrintJobHistoryAllowedByPolicy_ ||
        !this.printJobs_.length;
  },

  /** @private */
  onClearAllButtonUpdated_() {
    this.$.deleteIcon.classList.toggle(
        'delete-enabled', !this.shouldDisableClearAllButton_);
    this.$.deleteIcon.classList.toggle(
        'delete-disabled', this.shouldDisableClearAllButton_);
  }
});
