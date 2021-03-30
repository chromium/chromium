// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): use es6 module for mojo binding crbug/1004256
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://print-management/print_management.js';

import {setMetadataProviderForTesting} from 'chrome://print-management/mojo_interface_provider.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

const CompletionStatus = {
  FAILED: 0,
  CANCELED: 1,
  PRINTED: 2,
};

const ActivePrintJobState =
    chromeos.printing.printingManager.mojom.ActivePrintJobState;

const PrinterErrorCode = {
  NO_ERROR: chromeos.printing.printingManager.mojom.PrinterErrorCode.kNoError,
  PAPER_JAM: chromeos.printing.printingManager.mojom.PrinterErrorCode.kPaperJam,
  OUT_OF_PAPER:
      chromeos.printing.printingManager.mojom.PrinterErrorCode.kOutOfPaper,
  OUT_OF_INK:
      chromeos.printing.printingManager.mojom.PrinterErrorCode.kOutOfPaper,
  DOOR_OPEN: chromeos.printing.printingManager.mojom.PrinterErrorCode.kDoorOpen,
  PRINTER_UNREACHABLE: chromeos.printing.printingManager.mojom.PrinterErrorCode
                           .kPrinterUnreachable,
  TRAY_MISSING:
      chromeos.printing.printingManager.mojom.PrinterErrorCode.kTrayMissing,
  OUTPUT_FULL:
      chromeos.printing.printingManager.mojom.PrinterErrorCode.kOutputFull,
  STOPPED: chromeos.printing.printingManager.mojom.PrinterErrorCode.kStopped,
  FILTER_FAILED:
      chromeos.printing.printingManager.mojom.PrinterErrorCode.kFilterFailed,
  UNKNOWN_ERROR:
      chromeos.printing.printingManager.mojom.PrinterErrorCode.kUnknownError,
};

/**
 * Converts a JS string to mojo_base::mojom::String16 object.
 * @param {string} str
 * @return {!object}
 */
function strToMojoString16(str) {
  let arr = [];
  for (var i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }
  return {data: arr};
}

/**
 * Converts a JS time (milliseconds since UNIX epoch) to mojom::time
 * (microseconds since WINDOWS epoch).
 * @param {Date} jsDate
 * @return {number}
 */
function convertToMojoTime(jsDate) {
  const windowsEpoch = new Date(Date.UTC(1601, 0, 1, 0, 0, 0));
  const jsEpoch = new Date(Date.UTC(1970, 0, 1, 0, 0, 0));
  return ((jsEpoch - windowsEpoch) * 1000) + (jsDate.getTime() * 1000);
}

/**
 * Converts utf16 to a readable string.
 * @param {!object} arr
 * @return {string}
 */
function decodeString16(arr) {
  return arr.data.map(ch => String.fromCodePoint(ch)).join('');
}

/**
 * @param {string} id
 * @param {string} title
 * @param {number} date
 * @param {number} printerErrorCode
 * @param {?chromeos.printing.printingManager.mojom.CompletedPrintJobInfo}
 *     completedInfo
 * @param {?chromeos.printing.printingManager.mojom.ActivePrintJobInfo}
 *     activeInfo
 * @return {!Object}
 */
function createJobEntry(
    id, title, date, printerErrorCode, completedInfo, activeInfo) {
  // Assert that only one of either |completedInfo| or |activeInfo| is non-null.
  assertTrue(completedInfo ? !activeInfo : !!activeInfo);

  let jobEntry = {
    'id': id,
    'title': strToMojoString16(title),
    'creationTime': {internalValue: date},
    'printerName': strToMojoString16('printerName'),
    'printerUri': {url: '192.168.1.1'},
    'numberOfPages': 4,
    'printerErrorCode': printerErrorCode,
  };

  if (completedInfo) {
    jobEntry.completedInfo = completedInfo;
  } else {
    jobEntry.activePrintJobInfo = activeInfo;
  }
  return jobEntry;
}

/**
 * @param {number} completionStatus
 * @return {!chromeos.printing.printingManager.mojom.CompletedPrintJobInfo}
 */
function createCompletedPrintJobInfo(completionStatus) {
  let completedInfo = {'completionStatus': completionStatus};
  return completedInfo;
}

/**
 *
 * @param {number} printedPages
 * @param {!chromeos.printing.printingManager.mojom.ActivePrintJobState}
 *     activeState
 * @return {!chromeos.printing.printingManager.mojom.ActivePrintJobInfo}
 */
function createOngoingPrintJobInfo(printedPages, activeState) {
  let activeInfo = {
    'printedPages': printedPages,
    'activeState': activeState,
  };
  return activeInfo;
}

/**
 * @param{!Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
 *     expected
 * @param{!Array<!HTMLElement>} actual
 */
function verifyPrintJobs(expected, actual) {
  assertEquals(expected.length, actual.length);
  for (let i = 0; i < expected.length; i++) {
    const actualJobInfo = actual[i].jobEntry;
    assertEquals(expected[i].id, actualJobInfo.id);
    assertEquals(
        decodeString16(expected[i].title), decodeString16(actualJobInfo.title));
    assertEquals(
        Number(expected[i].creationTime.internalValue),
        Number(actualJobInfo.creationTime.internalValue));
    assertEquals(
        decodeString16(expected[i].printerName),
        decodeString16(actualJobInfo.printerName));
    assertEquals(expected[i].printerErrorCode, actualJobInfo.printerErrorCode);

    if (actualJobInfo.completedInfo) {
      assertEquals(
          expected[i].completedInfo.completionStatus,
          actualJobInfo.completedInfo.completionStatus);
    } else {
      assertEquals(
          expected[i].activePrintJobInfo.printedPages,
          actualJobInfo.activePrintJobInfo.printedPages);
      assertEquals(
          expected[i].activePrintJobInfo.activeState,
          actualJobInfo.activePrintJobInfo.activeState);
    }
  }
}

/**
 * @param {!HTMLElement} page
 * @return {!Array<!HTMLElement>}
 */
function getHistoryPrintJobEntries(page) {
  const entryList = page.$$('#entryList');
  return Array.from(
      entryList.querySelectorAll('print-job-entry:not([hidden])'));
}

/**
 * @param {!HTMLElement} page
 * @return {!Array<!HTMLElement>}
 */
function getOngoingPrintJobEntries(page) {
  assertTrue(page.$$('#ongoingEmptyState').hidden);
  const entryList = page.$$('#ongoingList');
  return Array.from(
      entryList.querySelectorAll('print-job-entry:not([hidden])'));
}

class FakePrintingMetadataProvider {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /**
     * @private {!Array<chromeos.printing.printingManager.mojom.PrintJobInfo>}
     */
    this.printJobs_ = [];

    /** @private boolean */
    this.shouldAttemptCancel_ = true;
    this.isAllowedByPolicy_ = true;

    /**
     * @private
     *     {?chromeos.printing.printingManager.mojom.PrintJobsObserverRemote}
     */
    this.printJobsObserverRemote_;

    /** @private {number} */
    this.expirationPeriod_ = 90;

    this.resetForTest();
  }

  resetForTest() {
    this.printJobs_ = [];
    this.shouldAttemptCancel_ = true;
    this.isAllowedByPolicy_ = true;
    if (this.printJobsObserverRemote_) {
      this.printJobsObserverRemote_ = null;
    }

    this.resolverMap_.set('getPrintJobs', new PromiseResolver());
    this.resolverMap_.set('deleteAllPrintJobs', new PromiseResolver());
    this.resolverMap_.set('observePrintJobs', new PromiseResolver());
    this.resolverMap_.set('cancelPrintJob', new PromiseResolver());
    this.resolverMap_.set(
        'getDeletePrintJobHistoryAllowedByPolicy', new PromiseResolver());
    this.resolverMap_.set(
        'getPrintJobHistoryExpirationPeriod', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    let method = this.resolverMap_.get(methodName);
    assertTrue(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  /**
   * @param {string} methodName
   * @protected
   */
  methodCalled(methodName) {
    this.getResolver_(methodName).resolve();
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.getResolver_(methodName).promise.then(() => {
      // Support sequential calls to whenCalled by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /**
   * @return
   *      {chromeos.printing.printingManager.mojom.PrintJobsObserverRemote}
   */
  getObserverRemote() {
    return this.printJobsObserverRemote_;
  }

  /**
   * @param {?Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
   *     printJobs
   */
  setPrintJobs(printJobs) {
    this.printJobs_ = printJobs;
  }

  /** @param {number} expirationPeriod */
  setExpirationPeriod(expirationPeriod) {
    this.expirationPeriod_ = expirationPeriod;
  }

  /**
   * @param {boolean} shouldAttemptCancel
   */
  setShouldAttemptCancel(shouldAttemptCancel) {
    this.shouldAttemptCancel_ = shouldAttemptCancel;
  }

  /**
   * @param {boolean} isAllowedByPolicy
   */
  setDeletePrintJobPolicy(isAllowedByPolicy) {
    this.isAllowedByPolicy_ = isAllowedByPolicy;
  }

  /**
   * @param {chromeos.printing.printingManager.mojom.PrintJobInfo} job
   */
  addPrintJob(job) {
    this.printJobs_ = this.printJobs_.concat(job);
  }

  simulatePrintJobsDeletedfromDatabase() {
    this.printJobs_ = [];
    this.printJobsObserverRemote_.onAllPrintJobsDeleted();
  }

  /**
   * @param {chromeos.printing.printingManager.mojom.PrintJobInfo} job
   */
  simulateUpdatePrintJob(job) {
    if (job.activePrintJobInfo.activeState ===
        ActivePrintJobState.kDocumentDone) {
      // Create copy of |job| to modify.
      let updatedJob = Object.assign({}, job);
      updatedJob.activePrintJobInfo = null;
      updatedJob.completedInfo =
          createCompletedPrintJobInfo(CompletionStatus.PRINTED);
      // Replace with updated print job.
      const idx =
          this.printJobs_.findIndex(arr_job => arr_job.id === updatedJob.id);
      if (idx !== -1) {
        this.printJobs_.splice(idx, 1, updatedJob);
      }
    }
    this.printJobsObserverRemote_.onPrintJobUpdate(job);
  }

  // printingMetadataProvider methods

  /**
   * @return {!Promise<{printJobs:
   *     !Array<chromeos.printing.printingManager.mojom.PrintJobInfo>}>}
   */
  getPrintJobs() {
    return new Promise(resolve => {
      this.methodCalled('getPrintJobs');
      resolve({printJobs: this.printJobs_ || []});
    });
  }

  /** @return {!Promise<{success: boolean}>} */
  deleteAllPrintJobs() {
    return new Promise(resolve => {
      this.printJobs_ = [];
      this.methodCalled('deleteAllPrintJobs');
      resolve({success: true});
    });
  }

  /** @return {!Promise<{isAllowedByPolicy: boolean}>} */
  getDeletePrintJobHistoryAllowedByPolicy() {
    return new Promise(resolve => {
      this.methodCalled('getDeletePrintJobHistoryAllowedByPolicy');
      resolve({isAllowedByPolicy: this.isAllowedByPolicy_});
    });
  }

  /** @return {!Promise<{expirationPeriod: number}>} */
  getPrintJobHistoryExpirationPeriod() {
    return new Promise(resolve => {
      this.methodCalled('getPrintJobHistoryExpirationPeriod');
      resolve({
        expirationPeriodInDays: this.expirationPeriod_,
        isFromPolicy: true,
      });
    });
  }

  /**
   * @param {!string} id
   * @return {!Promise<{attemptedCancel}>}
   */
  cancelPrintJob(id) {
    return new Promise(resolve => {
      this.methodCalled('cancelPrintJob');
      resolve({attempedCancel: this.shouldAttemptCancel_});
    });
  }

  /**
   * @param
   * {!chromeos.printing.printingManager.mojom.PrintJobsObserverRemote} remote
   * @return {!Promise}
   */
  observePrintJobs(remote) {
    return new Promise(resolve => {
      this.printJobsObserverRemote_ = remote;
      this.methodCalled('observePrintJobs');
      resolve();
    });
  }
}

suite('PrintManagementTest', () => {
  /** @type {?PrintManagementElement} */
  let page = null;

  /**
   * @type {
   *    ?chromeos.printing.printingManager.mojom.PrintingMetadataProviderRemote
   *  }
   */
  let mojoApi_;

  suiteSetup(() => {
    mojoApi_ = new FakePrintingMetadataProvider();
    setMetadataProviderForTesting(mojoApi_);
  });

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    mojoApi_.resetForTest();
    page.remove();
    page = null;
  });

  /**
   * @param {?Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
   *     printJobs
   * @return {!Promise}
   */
  function initializePrintManagementApp(printJobs) {
    mojoApi_.setPrintJobs(printJobs);
    page = document.createElement('print-management');
    document.body.appendChild(page);
    assertTrue(!!page);
    flush();
    return mojoApi_.whenCalled('observePrintJobs');
  }

  /**
   * @param {!HtmlElement} jobEntryElement
   * @param {FakePrintingMetadataProvider} mojoApi
   * @param {boolean} shouldAttemptCancel
   * @param {?Array<!chromeos.printing.printingManager.mojom.PrintJobInfo>}
   *    expectedHistoryList
   * @return {!Promise}
   */
  function simulateCancelPrintJob(
      jobEntryElement, mojoApi, shouldAttemptCancel, expectedHistoryList) {
    mojoApi.setShouldAttemptCancel(shouldAttemptCancel);

    const cancelButton = jobEntryElement.$$('#cancelPrintJobButton');
    cancelButton.click();
    return mojoApi.whenCalled('cancelPrintJob').then(() => {
      // Create copy of |jobEntryElement.jobEntry| to modify.
      let updatedJob = Object.assign({}, jobEntryElement.jobEntry);
      updatedJob.activePrintJobInfo = createOngoingPrintJobInfo(
          /*printedPages=*/ 0, ActivePrintJobState.kDocumentDone,
          PrinterErrorCode.NO_ERROR);
      // Simulate print jobs cancelled notification update sent.
      mojoApi.getObserverRemote().onPrintJobUpdate(updatedJob);

      // Simulate print job database updated with the canceled print job.
      mojoApi.setPrintJobs(expectedHistoryList);
      return mojoApi.whenCalled('getPrintJobs');
    });
  }
  test('PrintJobHistoryExpirationPeriodOneDay', () => {
    const completedInfo = createCompletedPrintJobInfo(CompletionStatus.PRINTED);
    const expectedText = 'Print jobs older than 1 day will be removed';
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
    ];
    // Print job metadata will be stored for 1 day.
    mojoApi_.setExpirationPeriod(1);
    return initializePrintManagementApp(expectedArr.slice().reverse())
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          return mojoApi_.whenCalled('getPrintJobHistoryExpirationPeriod');
        })
        .then(() => {
          const historyInfoTooltip = page.$$('paper-tooltip');
          assertEquals(expectedText, historyInfoTooltip.textContent.trim());
        });
  });

  test('PrintJobHistoryExpirationPeriodDefault', () => {
    const completedInfo = createCompletedPrintJobInfo(CompletionStatus.PRINTED);
    const expectedText = 'Print jobs older than 90 days will be removed';
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
    ];

    // Print job metadata will be stored for 90 days which is the default
    // period when the policy is not controlled.
    mojoApi_.setExpirationPeriod(90);
    return initializePrintManagementApp(expectedArr.slice().reverse())
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          return mojoApi_.whenCalled('getPrintJobHistoryExpirationPeriod');
        })
        .then(() => {
          const historyInfoTooltip = page.$$('paper-tooltip');
          assertEquals(expectedText, historyInfoTooltip.textContent.trim());
        });
  });

  test('PrintJobHistoryExpirationPeriodIndefinte', () => {
    const completedInfo = createCompletedPrintJobInfo(CompletionStatus.PRINTED);
    const expectedText = 'Print jobs will appear in history unless they are ' +
        'removed manually';
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
    ];

    // When this policy is set to a value of -1, the print jobs metadata is
    // stored indefinitely.
    mojoApi_.setExpirationPeriod(-1);
    return initializePrintManagementApp(expectedArr.slice().reverse())
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          return mojoApi_.whenCalled('getPrintJobHistoryExpirationPeriod');
        })
        .then(() => {
          const historyInfoTooltip = page.$$('paper-tooltip');
          assertEquals(expectedText, historyInfoTooltip.textContent.trim());
        });
  });

  test('PrintJobHistoryExpirationPeriodNDays', () => {
    const completedInfo = createCompletedPrintJobInfo(CompletionStatus.PRINTED);
    const expectedText = 'Print jobs older than 4 days will be removed';
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
    ];

    // Print job metadata will be stored for 4 days.
    mojoApi_.setExpirationPeriod(4);
    return initializePrintManagementApp(expectedArr.slice().reverse())
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          return mojoApi_.whenCalled('getPrintJobHistoryExpirationPeriod');
        })
        .then(() => {
          const historyInfoTooltip = page.$$('paper-tooltip');
          assertEquals(expectedText, historyInfoTooltip.textContent.trim());
        });
  });

  test('PrintHistoryListIsSortedReverseChronologically', () => {
    const completedInfo = createCompletedPrintJobInfo(CompletionStatus.PRINTED);
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
      createJobEntry(
          'middle', 'titleB',
          convertToMojoTime(new Date(Date.UTC(2020, 2, 1, 1, 1, 1))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
      createJobEntry(
          'oldest', 'titleC',
          convertToMojoTime(new Date(Date.UTC(2020, 1, 1, 1, 1, 1))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null)
    ];

    // Initialize with a reversed array of |expectedArr|, since we expect the
    // app to sort the list when it first loads. Since reverse() mutates the
    // original array, use a copy array to prevent mutating |expectedArr|.
    return initializePrintManagementApp(expectedArr.slice().reverse())
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          verifyPrintJobs(expectedArr, getHistoryPrintJobEntries(page));
        });
  });

  test('ClearAllButtonDisabledWhenNoPrintJobsSaved', () => {
    // Initialize with no saved print jobs, expect the clear all button to be
    // disabled.
    return initializePrintManagementApp(/*printJobs=*/[])
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          assertTrue(page.$$('#clearAllButton').disabled);
          assertTrue(!page.$$('#policyIcon'));
        });
  });

  test('ClearAllButtonDisabledByPolicy', () => {
    const expectedArr = [createJobEntry(
        'newest', 'titleA',
        convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
        PrinterErrorCode.NO_ERROR,
        createCompletedPrintJobInfo(CompletionStatus.PRINTED),
        /*activeInfo=*/ null)];
    // Set policy to prevent user from deleting history.
    mojoApi_.setDeletePrintJobPolicy(/*isAllowedByPolicy=*/ false);
    return initializePrintManagementApp(expectedArr)
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          assertTrue(page.$$('#clearAllButton').disabled);
          assertTrue(!!page.$$('#policyIcon'));
        });
  });

  test('ClearAllPrintHistory', () => {
    const completedInfo = createCompletedPrintJobInfo(CompletionStatus.PRINTED);
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date(Date('February 5, 2020 03:24:00'))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
      createJobEntry(
          'fileB', 'titleB',
          convertToMojoTime(new Date(Date('February 5, 2020 03:24:00'))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
      createJobEntry(
          'fileC', 'titleC',
          convertToMojoTime(new Date(Date('February 5, 2020 03:24:00'))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
    ];

    return initializePrintManagementApp(expectedArr)
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          return mojoApi_.whenCalled('getDeletePrintJobHistoryAllowedByPolicy');
        })
        .then(() => {
          flush();
          verifyPrintJobs(expectedArr, getHistoryPrintJobEntries(page));

          // Click the clear all button.
          const button = page.$$('#clearAllButton');
          button.click();
          flush();
          // Verify that the confirmation dialog shows up and click on the
          // confirmation button.
          const dialog = page.$$('#clearHistoryDialog');
          assertTrue(!!dialog);
          assertTrue(!dialog.$$('.action-button').disabled);
          dialog.$$('.action-button').click();
          assertTrue(dialog.$$('.action-button').disabled);
          return mojoApi_.whenCalled('deleteAllPrintJobs');
        })
        .then(() => {
          flush();
          // After clearing the history list, expect that the history list and
          // header are no longer
          assertTrue(!page.$$('#entryList'));
          assertTrue(!page.$$('#historyHeaderContainer'));
          assertTrue(page.$$('#clearAllButton').disabled);
        });
  });

  test('PrintJobDeletesFromObserver', () => {
    const completedInfo = createCompletedPrintJobInfo(CompletionStatus.PRINTED);
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date(Date('February 5, 2020 03:24:00'))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
      createJobEntry(
          'fileB', 'titleB',
          convertToMojoTime(new Date(Date('February 6, 2020 03:24:00'))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
      createJobEntry(
          'fileC', 'titleC',
          convertToMojoTime(new Date(Date('February 7, 2020 03:24:00'))),
          PrinterErrorCode.NO_ERROR, completedInfo, /*activeInfo=*/ null),
    ];

    return initializePrintManagementApp(expectedArr)
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          verifyPrintJobs(expectedArr, getHistoryPrintJobEntries(page));

          // Simulate observer call that signals all print jobs have been
          // deleted. Expect the UI to retrieve an empty list of print jobs.
          mojoApi_.simulatePrintJobsDeletedfromDatabase();
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          // After clearing the history list, expect that the history list and
          // header are no longer
          assertTrue(!page.$$('#entryList'));
          assertTrue(!page.$$('#historyHeaderContainer'));
          assertTrue(page.$$('#clearAllButton').disabled);
        });
  });

  test('HistoryHeaderIsHiddenWithEmptyPrintJobsInHistory', () => {
    return initializePrintManagementApp(/*expectedArr=*/[])
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          // Header should be not be rendered since no there are no completed
          // print jobs in the history.
          assertTrue(!page.$$('#historyHeaderContainer'));
        });
  });

  test('LoadsOngoingPrintJob', () => {
    const activeInfo1 = createOngoingPrintJobInfo(
        /*printedPages=*/ 0, ActivePrintJobState.kStarted);
    const activeInfo2 = createOngoingPrintJobInfo(
        /*printedPages=*/ 1, ActivePrintJobState.kStarted);
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date(Date('February 5, 2020 03:23:00'))),
          PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null, activeInfo1),
      createJobEntry(
          'fileB', 'titleB',
          convertToMojoTime(new Date(Date('February 5, 2020 03:24:00'))),
          PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null, activeInfo2),
    ];

    return initializePrintManagementApp(expectedArr)
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          verifyPrintJobs(expectedArr, getOngoingPrintJobEntries(page));
        });
  });

  test('OngoingPrintJobUpdated', () => {
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date('February 5, 2020 03:24:00')),
          PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null,
          createOngoingPrintJobInfo(
              /*printedPages=*/ 0, ActivePrintJobState.kStarted)),
    ];

    const activeInfo2 = createOngoingPrintJobInfo(
        /*printedPages=*/ 1, ActivePrintJobState.kStarted);
    const expectedUpdatedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date(Date('February 5, 2020 03:24:00'))),
          PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null, activeInfo2),
    ];

    return initializePrintManagementApp(expectedArr)
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          verifyPrintJobs(expectedArr, getOngoingPrintJobEntries(page));
          mojoApi_.simulateUpdatePrintJob(expectedUpdatedArr[0]);
          return flushTasks();
        })
        .then(() => {
          flush();
          verifyPrintJobs(expectedUpdatedArr, getOngoingPrintJobEntries(page));
        });
  });

  test('OngoingPrintJobUpdatedToStopped', () => {
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date('February 5, 2020 03:24:00')),
          PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null,
          createOngoingPrintJobInfo(
              /*printedPages=*/ 0, ActivePrintJobState.kStarted)),
    ];

    const activeInfo2 = createOngoingPrintJobInfo(
        /*printedPages=*/ 0, ActivePrintJobState.kStarted);
    const expectedUpdatedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date('February 5, 2020 03:24:00')),
          PrinterErrorCode.OUT_OF_PAPER, /*completedInfo=*/ null, activeInfo2),
    ];

    return initializePrintManagementApp(expectedArr)
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          verifyPrintJobs(expectedArr, getOngoingPrintJobEntries(page));
          mojoApi_.simulateUpdatePrintJob(expectedUpdatedArr[0]);
          return flushTasks();
        })
        .then(() => {
          flush();
          verifyPrintJobs(expectedUpdatedArr, getOngoingPrintJobEntries(page));
        });
  });

  test('NewOngoingPrintJobsDetected', () => {
    const initialJob = [createJobEntry(
        'fileA', 'titleA',
        convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 0, ActivePrintJobState.kStarted))];

    const newOngoingJob = createJobEntry(
        'fileB', 'titleB',
        convertToMojoTime(new Date(Date('February 5, 2020 03:25:00'))),
        PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1, ActivePrintJobState.kStarted));

    return initializePrintManagementApp(initialJob)
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          verifyPrintJobs(initialJob, getOngoingPrintJobEntries(page));
          mojoApi_.simulateUpdatePrintJob(newOngoingJob);
          return flushTasks();
        })
        .then(() => {
          flush();
          verifyPrintJobs(
              [initialJob[0], newOngoingJob], getOngoingPrintJobEntries(page));
        });
  });

  test('OngoingPrintJobCompletesAndUpdatesHistoryList', () => {
    const id = 'fileA';
    const title = 'titleA';
    const date = convertToMojoTime(new Date(Date('February 5, 2020 03:24:00')));

    const activeJob = createJobEntry(
        id, title, date, PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 0, ActivePrintJobState.kStarted));

    const expectedPrintJobArr = [createJobEntry(
        id, title, date, PrinterErrorCode.NO_ERROR,
        createCompletedPrintJobInfo(CompletionStatus.PRINTED),
        /*activeInfo=*/ '')];

    return initializePrintManagementApp([activeJob])
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          verifyPrintJobs([activeJob], getOngoingPrintJobEntries(page));
          // Simulate ongoing print job has completed.
          activeJob.activePrintJobInfo.activeState =
              ActivePrintJobState.kDocumentDone;
          mojoApi_.simulateUpdatePrintJob(activeJob);
          // Simulate print job has been added to history.
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          verifyPrintJobs(expectedPrintJobArr, getHistoryPrintJobEntries(page));
        });
  });

  test('OngoingPrintJobEmptyState', () => {
    return initializePrintManagementApp(/*expectedArr=*/[])
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          // Assert that ongoing list is empty and the empty state message is
          // not hidden.
          assertTrue(!page.$$('#ongoingList'));
          assertTrue(!page.$$('#ongoingEmptyState').hidden);
        });
  });

  test('CancelOngoingPrintJob', () => {
    const kId = 'fileA';
    const kTitle = 'titleA';
    const kTime =
        convertToMojoTime(new Date(Date('February 5, 2020 03:23:00')));
    const expectedArr = [
      createJobEntry(
          kId, kTitle, kTime, PrinterErrorCode.NO_ERROR,
          /*completedInfo=*/ null,
          createOngoingPrintJobInfo(
              /*printedPages=*/ 0, ActivePrintJobState.STARTED)),
    ];

    const expectedHistoryList = [createJobEntry(
        kId, kTitle, kTime, PrinterErrorCode.NO_ERROR,
        createCompletedPrintJobInfo(CompletionStatus.CANCELED))];

    return initializePrintManagementApp(expectedArr)
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          let jobEntries = getOngoingPrintJobEntries(page);
          verifyPrintJobs(expectedArr, jobEntries);

          return simulateCancelPrintJob(
              jobEntries[0], mojoApi_,
              /*shouldAttemptCancel*/ true, expectedHistoryList);
        })
        .then(() => {
          flush();
          // Verify that there are no ongoing print jobs and history list is
          // populated.
          assertTrue(!page.$$('#ongoingList'));
          verifyPrintJobs(expectedHistoryList, getHistoryPrintJobEntries(page));
        });
  });

  test('CancelOngoingPrintJobNotAttempted', () => {
    const kId = 'fileA';
    const kTitle = 'titleA';
    const kTime =
        convertToMojoTime(new Date(Date('February 5, 2020 03:23:00')));

    const expectedArr = [
      createJobEntry(
          kId, kTitle, kTime, PrinterErrorCode.NO_ERROR,
          /*completedInfo=*/ null,
          createOngoingPrintJobInfo(
              /*printedPages=*/ 0, ActivePrintJobState.STARTED)),
    ];

    const expectedHistoryList = [createJobEntry(
        kId, kTitle, kTime, PrinterErrorCode.NO_ERROR,
        createCompletedPrintJobInfo(CompletionStatus.CANCELED))];

    return initializePrintManagementApp(expectedArr)
        .then(() => {
          return mojoApi_.whenCalled('getPrintJobs');
        })
        .then(() => {
          flush();
          let jobEntries = getOngoingPrintJobEntries(page);
          verifyPrintJobs(expectedArr, jobEntries);

          return simulateCancelPrintJob(
              jobEntries[0], mojoApi_,
              /*shouldAttemptCancel=*/ false, expectedHistoryList);
        })
        .then(() => {
          flush();
          // Verify that there are no ongoing print jobs and history list is
          // populated.
          // TODO(crbug/1093527): Show error message to user after UX guidance.
          assertTrue(!page.$$('#ongoingList'));
          verifyPrintJobs(expectedHistoryList, getHistoryPrintJobEntries(page));
        });
  });
});

suite('PrintJobEntryTest', () => {
  /** @type {?HTMLElement} */
  let jobEntryTestElement = null;

  /**
   * @type {
   *    ?chromeos.printing.printingManager.mojom.PrintingMetadataProviderRemote
   *  }
   */
  let mojoApi_;

  suiteSetup(() => {
    mojoApi_ = new FakePrintingMetadataProvider();
    setMetadataProviderForTesting(mojoApi_);
  });

  setup(() => {
    jobEntryTestElement = document.createElement('print-job-entry');
    assertTrue(!!jobEntryTestElement);
    document.body.appendChild(jobEntryTestElement);
  });

  teardown(() => {
    jobEntryTestElement.remove();
    jobEntryTestElement = null;
  });

  /**
   * @param {!HTMLElement} element
   * @param {number} newStatus
   * @param {string} expectedStatus
   */
  function updateAndVerifyCompletionStatus(element, newStatus, expectedStatus) {
    element.set('jobEntry.completedInfo.completionStatus', newStatus);
    assertEquals(
        expectedStatus, element.$$('#completionStatus').textContent.trim());
  }

  test('initializeJobEntry', () => {
    const expectedTitle = 'title.pdf';
    const expectedStatus = CompletionStatus.PRINTED;
    const expectedPrinterError = PrinterErrorCode.NO_ERROR;
    const expectedCreationTime = convertToMojoTime(new Date());

    const completedInfo = createCompletedPrintJobInfo(expectedStatus);
    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', expectedTitle, expectedCreationTime, expectedPrinterError,
        completedInfo, /*activeInfo=*/ null);

    flush();

    // Assert the title, creation time, and status are displayed correctly.
    assertEquals(
        expectedTitle, jobEntryTestElement.$$('#jobTitle').textContent.trim());
    assertEquals(
        'Printed',
        jobEntryTestElement.$$('#completionStatus').textContent.trim());
    // Verify correct icon is shown.
    assertEquals(
        'print-management:file-pdf', jobEntryTestElement.$$('#fileIcon').icon);

    // Change date and assert it shows the correct date (Feb 5, 2020);
    jobEntryTestElement.set('jobEntry.creationTime', {
      internalValue: convertToMojoTime(new Date('February 5, 2020 03:24:00'))
    });
    assertEquals(
        'Feb 5, 2020',
        jobEntryTestElement.$$('#creationTime').textContent.trim());
  });

  test('initializeFailedJobEntry', () => {
    const expectedTitle = 'titleA.doc';
    // Create a print job with a failed status with error: out of paper.
    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', expectedTitle,
        convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.OUT_OF_PAPER,
        createCompletedPrintJobInfo(CompletionStatus.FAILED),
        /*activeInfo=*/ null);

    flush();

    // Assert the title, creation time, and status are displayed correctly.
    assertEquals(
        expectedTitle, jobEntryTestElement.$$('#jobTitle').textContent.trim());
    assertEquals(
        'Failed - Out of paper',
        jobEntryTestElement.$$('#completionStatus').textContent.trim());
    // Verify correct icon is shown.
    assertEquals(
        'print-management:file-word', jobEntryTestElement.$$('#fileIcon').icon);
    assertEquals(
        'Feb 5, 2020',
        jobEntryTestElement.$$('#creationTime').textContent.trim());
  });

  test('initializeOngoingJobEntry', () => {
    const expectedTitle = 'title';
    const expectedCreationTime =
        convertToMojoTime(new Date('February 5, 2020 03:24:00'));
    const expectedPrinterError = ActivePrintJobState.kStarted;

    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', expectedTitle, expectedCreationTime,
        PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null,
        createOngoingPrintJobInfo(/*printedPages=*/ 1, expectedPrinterError));

    flush();

    // Assert the title, creation time, and status are displayed correctly.
    assertEquals(
        expectedTitle, jobEntryTestElement.$$('#jobTitle').textContent.trim());
    assertEquals(
        'Feb 5, 2020',
        jobEntryTestElement.$$('#creationTime').textContent.trim());
    assertEquals(
        '1/4', jobEntryTestElement.$$('#numericalProgress').textContent.trim());
    assertEquals(
        'print-management:file-generic',
        jobEntryTestElement.$$('#fileIcon').icon);
  });

  test('initializeStoppedOngoingJobEntry', () => {
    const expectedTitle = 'title';
    const expectedCreationTime =
        convertToMojoTime(new Date('February 5, 2020 03:24:00'));
    const expectedPrinterError = ActivePrintJobState.kStarted;
    const expectedOngoingError = PrinterErrorCode.OUT_OF_PAPER;

    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', expectedTitle, expectedCreationTime, expectedOngoingError,
        /*completedInfo=*/ null,
        createOngoingPrintJobInfo(/*printedPages=*/ 1, expectedPrinterError));

    flush();

    // Assert the title, creation time, and status are displayed correctly.
    assertEquals(
        expectedTitle, jobEntryTestElement.$$('#jobTitle').textContent.trim());
    assertEquals(
        'Feb 5, 2020',
        jobEntryTestElement.$$('#creationTime').textContent.trim());
    assertEquals(
        'Stopped - Out of paper',
        jobEntryTestElement.$$('#ongoingError').textContent.trim());
    assertEquals(
        'print-management:file-generic',
        jobEntryTestElement.$$('#fileIcon').icon);
  });

  test('ensureGoogleFileIconIsShown', () => {
    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', /*fileName=*/ '.test - Google Docs',
        /*date=*/ convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1,
            /*printerError=*/ ActivePrintJobState.kStarted));

    flush();

    assertEquals(
        'print-management:file-gdoc', jobEntryTestElement.$$('#fileIcon').icon);
  });

  test('ensureGenericFileIconIsShown', () => {
    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', /*fileName=*/ '.test',
        /*date=*/ convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.NO_ERROR, /*completedInfo=*/ null,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1,
            /*printerError=*/ ActivePrintJobState.kStarted));

    flush();

    assertEquals(
        'print-management:file-generic',
        jobEntryTestElement.$$('#fileIcon').icon);
  });
});
