// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scanning_app.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {setScanServiceForTesting} from 'chrome://scanning/mojo_interface_provider.js';
import {ScannerArr} from 'chrome://scanning/scanning_app_types.js';
import {tokenToString} from 'chrome://scanning/scanning_app_util.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

import {changeSelect, createScanner, createScannerSource} from './scanning_app_test_utils.js';
import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

const MY_FILES_PATH = '/home/chronos/user/MyFiles';

const ColorMode = {
  BLACK_AND_WHITE: ash.scanning.mojom.ColorMode.kBlackAndWhite,
  GRAYSCALE: ash.scanning.mojom.ColorMode.kGrayscale,
  COLOR: ash.scanning.mojom.ColorMode.kColor,
};

const FileType = {
  JPG: ash.scanning.mojom.FileType.kJpg,
  PDF: ash.scanning.mojom.FileType.kPdf,
  PNG: ash.scanning.mojom.FileType.kPng,
};

const PageSize = {
  A4: ash.scanning.mojom.PageSize.kIsoA4,
  Letter: ash.scanning.mojom.PageSize.kNaLetter,
  Max: ash.scanning.mojom.PageSize.kMax,
};

const SourceType = {
  FLATBED: ash.scanning.mojom.SourceType.kFlatbed,
  ADF_SIMPLEX: ash.scanning.mojom.SourceType.kAdfSimplex,
  ADF_DUPLEX: ash.scanning.mojom.SourceType.kAdfDuplex,
};

const pageSizes = [PageSize.A4, PageSize.Letter, PageSize.Max];

const firstScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 1});
const firstScannerName = 'Scanner 1';

const secondScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 2});
const secondScannerName = 'Scanner 2';

const firstCapabilities = {
  sources: [
    createScannerSource(SourceType.ADF_DUPLEX, 'adf duplex', pageSizes),
    createScannerSource(SourceType.FLATBED, 'platen', pageSizes),
  ],
  colorModes: [ColorMode.BLACK_AND_WHITE, ColorMode.COLOR],
  resolutions: [75, 100, 300]
};

const secondCapabilities = {
  sources:
      [createScannerSource(SourceType.ADF_SIMPLEX, 'adf simplex', pageSizes)],
  colorModes: [ColorMode.GRAYSCALE],
  resolutions: [150, 600]
};

/** @implements {ash.scanning.mojom.ScanServiceInterface} */
class FakeScanService {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /** @private {!ScannerArr} */
    this.scanners_ = [];

    /**
     * @private {!Map<!mojoBase.mojom.UnguessableToken,
     *     !ash.scanning.mojom.ScannerCapabilities>}
     */
    this.capabilities_ = new Map();

    /** @private {?ash.scanning.mojom.ScanJobObserverRemote} */
    this.scanJobObserverRemote_ = null;

    /** @private {boolean} */
    this.failStartScan_ = false;

    this.resetForTest();
  }

  resetForTest() {
    this.scanners_ = [];
    this.capabilities_ = new Map();
    this.scanJobObserverRemote_ = null;
    this.failStartScan_ = false;
    this.resolverMap_.set('getScanners', new PromiseResolver());
    this.resolverMap_.set('getScannerCapabilities', new PromiseResolver());
    this.resolverMap_.set('startScan', new PromiseResolver());
    this.resolverMap_.set('cancelScan', new PromiseResolver());
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
      // Support sequential calls to whenCalled() by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /** @param {!ScannerArr} scanners */
  setScanners(scanners) {
    this.scanners_ = scanners;
  }

  /** @param {ash.scanning.mojom.Scanner} scanner */
  addScanner(scanner) {
    this.scanners_ = this.scanners_.concat(scanner);
  }

  /**
   * @param {!Map<!mojoBase.mojom.UnguessableToken,
   *     !ash.scanning.mojom.ScannerCapabilities>} capabilities
   */
  setCapabilities(capabilities) {
    this.capabilities_ = capabilities;
  }

  /** @param {boolean} failStartScan */
  setFailStartScan(failStartScan) {
    this.failStartScan_ = failStartScan;
  }

  /**
   * @param {number} pageNumber
   * @param {number} progressPercent
   * @return {!Promise}
   */
  simulateProgress(pageNumber, progressPercent) {
    this.scanJobObserverRemote_.onPageProgress(pageNumber, progressPercent);
    return flushTasks();
  }

  /**
   * @param {number} pageNumber
   * @return {!Promise}
   */
  simulatePageComplete(pageNumber) {
    this.scanJobObserverRemote_.onPageProgress(pageNumber, 100);
    const fakePageData = [2, 57, 13, 289];
    this.scanJobObserverRemote_.onPageComplete(fakePageData);
    return flushTasks();
  }

  /**
   * @param {!ash.scanning.mojom.ScanResult} result
   * @param {!Array<!mojoBase.mojom.FilePath>} scannedFilePaths
   * @return {!Promise}
   */
  simulateScanComplete(result, scannedFilePaths) {
    this.scanJobObserverRemote_.onScanComplete(result, scannedFilePaths);
    return flushTasks();
  }

  /**
   * @param {boolean} success
   * @return {!Promise}
   */
  simulateCancelComplete(success) {
    this.scanJobObserverRemote_.onCancelComplete(success);
    return flushTasks();
  }

  // scanService methods:

  /** @return {!Promise<{scanners: !ScannerArr}>} */
  getScanners() {
    return new Promise(resolve => {
      this.methodCalled('getScanners');
      resolve({scanners: this.scanners_ || []});
    });
  }

  /**
   * @param {!mojoBase.mojom.UnguessableToken} scanner_id
   * @return {!Promise<{capabilities:
   *    !ash.scanning.mojom.ScannerCapabilities}>}
   */
  getScannerCapabilities(scanner_id) {
    return new Promise(resolve => {
      this.methodCalled('getScannerCapabilities');
      resolve({capabilities: this.capabilities_.get(scanner_id)});
    });
  }

  /**
   * @param {!mojoBase.mojom.UnguessableToken} scanner_id
   * @param {!ash.scanning.mojom.ScanSettings} settings
   * @param {!ash.scanning.mojom.ScanJobObserverRemote} remote
   * @return {!Promise<{success: boolean}>}
   */
  startScan(scanner_id, settings, remote) {
    return new Promise(resolve => {
      this.scanJobObserverRemote_ = remote;
      this.methodCalled('startScan');
      resolve({success: !this.failStartScan_});
    });
  }

  cancelScan() {
    this.methodCalled('cancelScan');
  }
}

export function scanningAppTest() {
  /** @type {?ScanningAppElement} */
  let scanningApp = null;

  /** @type {?FakeScanService} */
  let fakeScanService_ = null;

  /** @type {?TestScanningBrowserProxy} */
  let testBrowserProxy = null;

  /** @type {?HTMLSelectElement} */
  let scannerSelect = null;

  /** @type {?HTMLSelectElement} */
  let sourceSelect = null;

  /** @type {?HTMLSelectElement} */
  let scanToSelect = null;

  /** @type {?HTMLSelectElement} */
  let fileTypeSelect = null;

  /** @type {?HTMLSelectElement} */
  let colorModeSelect = null;

  /** @type {?HTMLSelectElement} */
  let pageSizeSelect = null;

  /** @type {?HTMLSelectElement} */
  let resolutionSelect = null;

  /** @type {?CrButtonElement} */
  let scanButton = null;

  /** @type {?CrButtonElement} */
  let cancelButton = null;

  /** @type {?HTMLElement} */
  let helperText = null;

  /** @type {?HTMLElement} */
  let scanProgress = null;

  /** @type {?HTMLElement} */
  let progressText = null;

  /** @type {?HTMLElement} */
  let progressBar = null;

  /** @type {?HTMLElement} */
  let scannedImages = null;

  /**
   * @type {!Map<!mojoBase.mojom.UnguessableToken,
   *     !ash.scanning.mojom.ScannerCapabilities>}
   */
  const capabilities = new Map();
  capabilities.set(firstScannerId, firstCapabilities);
  capabilities.set(secondScannerId, secondCapabilities);

  /** @type {!ScannerArr} */
  const expectedScanners = [
    createScanner(firstScannerId, firstScannerName),
    createScanner(secondScannerId, secondScannerName)
  ];

  suiteSetup(() => {
    fakeScanService_ = new FakeScanService();
    setScanServiceForTesting(fakeScanService_);
    testBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.instance_ = testBrowserProxy;
    testBrowserProxy.setMyFilesPath(MY_FILES_PATH);
  });

  setup(function() {
    document.body.innerHTML = '';
  });

  teardown(function() {
    fakeScanService_.resetForTest();
    scanningApp.remove();
    scanningApp = null;
    scannerSelect = null;
    sourceSelect = null;
    scanToSelect = null;
    fileTypeSelect = null;
    colorModeSelect = null;
    pageSizeSelect = null;
    resolutionSelect = null;
    scanButton = null;
    cancelButton = null;
    helperText = null;
    scanProgress = null;
    progressText = null;
    progressBar = null;
    scannedImages = null;
  });

  /**
   * @param {!ScannerArr} scanners
   * @param {!Map<!mojoBase.mojom.UnguessableToken,
   *     !ash.scanning.mojom.ScannerCapabilities>} capabilities
   * @return {!Promise}
   */
  function initializeScanningApp(scanners, capabilities) {
    fakeScanService_.setScanners(scanners);
    fakeScanService_.setCapabilities(capabilities);
    scanningApp = /** @type {!ScanningAppElement} */ (
        document.createElement('scanning-app'));
    document.body.appendChild(scanningApp);
    assertTrue(!!scanningApp);
    assertTrue(isVisible(
        /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));
    return fakeScanService_.whenCalled('getScanners');
  }

  /**
   * Returns the "More settings" button.
   * @return {!CrButtonElement}
   */
  function getMoreSettingsButton() {
    const button =
        /** @type {!CrButtonElement} */ (scanningApp.$$('#moreSettingsButton'));
    assertTrue(!!button);
    return button;
  }

  /**
   * Clicks the "More settings" button.
   * @return {!Promise}
   */
  function clickMoreSettingsButton() {
    getMoreSettingsButton().click();
    return flushTasks();
  }

  /**
   * Clicks the "Done" button.
   * @return {!Promise}
   */
  function clickDoneButton() {
    const button = scanningApp.$$('scan-done-section').$$('#doneButton');
    assertTrue(!!button);
    button.click();
    return flushTasks();
  }

  /**
   * Clicks the "Ok" button to close the scan failed dialog.
   * @return {!Promise}
   */
  function clickOkButton() {
    const button = scanningApp.$$('#okButton');
    assertTrue(!!button);
    button.click();
    return flushTasks();
  }

  /**
   * Returns whether the "More settings" section is expanded or not.
   * @return {boolean}
   */
  function isSettingsOpen() {
    return scanningApp.$$('#collapse').opened;
  }

  test('Scan', () => {
    /** @type {!Array<!mojoBase.mojom.FilePath>} */
    const scannedFilePaths =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));

          scannerSelect = scanningApp.$$('#scannerSelect').$$('select');
          sourceSelect = scanningApp.$$('#sourceSelect').$$('select');
          scanToSelect = scanningApp.$$('#scanToSelect').$$('select');
          fileTypeSelect = scanningApp.$$('#fileTypeSelect').$$('select');
          colorModeSelect = scanningApp.$$('#colorModeSelect').$$('select');
          pageSizeSelect = scanningApp.$$('#pageSizeSelect').$$('select');
          resolutionSelect = scanningApp.$$('#resolutionSelect').$$('select');
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          cancelButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#cancelButton'));
          helperText = scanningApp.$$('#scanPreview').$$('#helperText');
          scanProgress = scanningApp.$$('#scanPreview').$$('#scanProgress');
          progressText = scanningApp.$$('#scanPreview').$$('#progressText');
          progressBar = scanningApp.$$('#scanPreview').$$('paper-progress');
          scannedImages = scanningApp.$$('#scanPreview').$$('#scannedImages');
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId), scanningApp.selectedScannerId);
          // A scanner with type "FLATBED" will be used as the selectedSource
          // if it exists.
          assertEquals(
              firstCapabilities.sources[1].name, scanningApp.selectedSource);
          assertEquals(MY_FILES_PATH, scanningApp.selectedFilePath);
          assertEquals(FileType.PDF.toString(), scanningApp.selectedFileType);
          assertEquals(
              ColorMode.COLOR.toString(), scanningApp.selectedColorMode);
          assertEquals(
              firstCapabilities.sources[0].pageSizes[1].toString(),
              scanningApp.selectedPageSize);
          assertEquals(
              firstCapabilities.resolutions[0].toString(),
              scanningApp.selectedResolution);

          // Before the scan button is clicked, the settings and scan button
          // should be enabled, and the helper text should be displayed.
          assertFalse(scannerSelect.disabled);
          assertFalse(sourceSelect.disabled);
          assertFalse(scanToSelect.disabled);
          assertFalse(fileTypeSelect.disabled);
          assertFalse(colorModeSelect.disabled);
          assertFalse(pageSizeSelect.disabled);
          assertFalse(resolutionSelect.disabled);
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertFalse(
              isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertTrue(isVisible(/** @type {!HTMLElement} */ (helperText)));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (scanProgress)));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));

          // Click the Scan button and wait till the scan is started.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          // After the scan button is clicked and the scan has started, the
          // settings and scan button should be disabled, and the progress bar
          // and text should be visible and indicate that scanning is in
          // progress.
          assertTrue(scannerSelect.disabled);
          assertTrue(sourceSelect.disabled);
          assertTrue(scanToSelect.disabled);
          assertTrue(fileTypeSelect.disabled);
          assertTrue(colorModeSelect.disabled);
          assertTrue(pageSizeSelect.disabled);
          assertTrue(resolutionSelect.disabled);
          assertTrue(scanButton.disabled);
          assertFalse(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (helperText)));
          assertTrue(isVisible(/** @type {!HTMLElement} */ (scanProgress)));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));
          assertEquals('Scanning page 1', progressText.textContent.trim());
          assertEquals(0, progressBar.value);

          // Simulate a progress update and verify the progress bar and text are
          // updated correctly.
          return fakeScanService_.simulateProgress(1, 17);
        })
        .then(() => {
          assertEquals('Scanning page 1', progressText.textContent.trim());
          assertEquals(17, progressBar.value);

          // Simulate a page complete update and verify the progress bar and
          // text are updated correctly.
          return fakeScanService_.simulatePageComplete(1);
        })
        .then(() => {
          assertEquals('Scanning page 1', progressText.textContent.trim());
          assertEquals(100, progressBar.value);

          // Simulate a progress update for a second page and verify the
          // progress bar and text are updated correctly.
          return fakeScanService_.simulateProgress(2, 53);
        })
        .then(() => {
          assertEquals('Scanning page 2', progressText.textContent.trim());
          assertEquals(53, progressBar.value);

          // Complete the page.
          return fakeScanService_.simulatePageComplete(2);
        })
        .then(() => {
          // Complete the scan.
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kSuccess, scannedFilePaths);
        })
        .then(() => {
          assertTrue(isVisible(/** @type {!HTMLElement} */ (scannedImages)));
          assertEquals(2, scannedImages.querySelectorAll('img').length);
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));
          assertArrayEquals(
              scannedFilePaths,
              scanningApp.$$('scan-done-section').scannedFilePaths);

          // Click the Done button to return to READY state.
          return clickDoneButton();
        })
        .then(() => {
          // After scanning is complete, the settings and scan button should be
          // enabled. The progress bar and text should no longer be visible.
          assertFalse(scannerSelect.disabled);
          assertFalse(sourceSelect.disabled);
          assertFalse(scanToSelect.disabled);
          assertFalse(fileTypeSelect.disabled);
          assertFalse(colorModeSelect.disabled);
          assertFalse(pageSizeSelect.disabled);
          assertFalse(resolutionSelect.disabled);
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertFalse(
              isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertTrue(isVisible(/** @type {!HTMLElement} */ (helperText)));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (scanProgress)));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (scannedImages)));
          assertEquals(0, scannedImages.querySelectorAll('img').length);
        });
  });

  test('ScanFailed', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          // Click the Scan button and wait till the scan is started.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          // Simulate a progress update.
          return fakeScanService_.simulateProgress(1, 17);
        })
        .then(() => {
          // Simulate the scan failing.
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kIoError, []);
        })
        .then(() => {
          // The scan failed dialog should open.
          assertTrue(scanningApp.$$('#scanFailedDialog').open);
          // Click the dialog's Ok button to return to READY state.
          return clickOkButton();
        })
        .then(() => {
          // After the dialog closes, the scan button should be enabled and
          // ready to start a new scan.
          assertFalse(scanningApp.$$('#scanFailedDialog').open);
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
        });
  });

  test('ScanResults', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kUnknownError, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogUnknownErrorText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kDeviceBusy, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogDeviceBusyText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kAdfJammed, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogAdfJammedText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kAdfEmpty, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogAdfEmptyText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kFlatbedOpen, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogFlatbedOpenText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        })
        .then(() => {
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kIoError, []);
        })
        .then(() => {
          assertEquals(
              loadTimeData.getString('scanFailedDialogIoErrorText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());
          return clickOkButton();
        });
  });

  test('CancelScan', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          cancelButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#cancelButton'));
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          // Before the scan button is clicked, the scan button should be
          // visible and enabled, and the cancel button shouldn't be visible.
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertFalse(
              isVisible(/** @type {!CrButtonElement} */ (cancelButton)));

          // Click the Scan button and wait till the scan is started.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          // After the scan button is clicked and the scan has started, the scan
          // button should be disabled and not visible, and the cancel button
          // should be visible.
          assertTrue(scanButton.disabled);
          assertFalse(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (cancelButton)));

          // Simulate a progress update and verify the progress bar and text are
          // updated correctly.
          return fakeScanService_.simulateProgress(1, 17);
        })
        .then(() => {
          // Click the cancel button to cancel the scan.
          cancelButton.click();
          return fakeScanService_.whenCalled('cancelScan');
        })
        .then(() => {
          // Cancel button should be disabled while canceling is in progress.
          assertTrue(cancelButton.disabled);
          // Simulate cancel completing successfully.
          return fakeScanService_.simulateCancelComplete(true);
        })
        .then(() => {
          // After canceling is complete, the scan button should be visible and
          // enabled, and the cancel button shouldn't be visible.
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
          assertFalse(
              isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertTrue(scanningApp.$$('#toast').open);
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#toastInfoIcon'))));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#getHelpLink'))));
          assertEquals(
              scanningApp.i18n('scanCanceledToastText'),
              scanningApp.$$('#toastText').textContent.trim());
        });
  });

  test('CancelScanFailed', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          cancelButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#cancelButton'));
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          // Click the Scan button and wait till the scan is started.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          // Simulate a progress update and verify the progress bar and text are
          // updated correctly.
          return fakeScanService_.simulateProgress(1, 17);
        })
        .then(() => {
          // Click the cancel button to cancel the scan.
          cancelButton.click();
          assertFalse(scanningApp.$$('#toast').open);
          return fakeScanService_.whenCalled('cancelScan');
        })
        .then(() => {
          // Cancel button should be disabled while canceling is in progress.
          assertTrue(cancelButton.disabled);
          // Simulate cancel failing.
          return fakeScanService_.simulateCancelComplete(false);
        })
        .then(() => {
          // After canceling fails, the error toast should pop up.
          assertTrue(scanningApp.$$('#toast').open);
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#toastInfoIcon'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#getHelpLink'))));
          assertEquals(
              scanningApp.i18n('cancelFailedToastText'),
              scanningApp.$$('#toastText').textContent.trim());
          // The scan progress page should still be showing with the cancel
          // button visible.
          assertTrue(
              isVisible(scanningApp.$$('#scanPreview').$$('#scanProgress')));
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (cancelButton)));
          assertFalse(
              isVisible(scanningApp.$$('#scanPreview').$$('#helperText')));
          assertFalse(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
        });
  });

  test('ScanFailedToStart', () => {
    fakeScanService_.setFailStartScan(true);

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          assertFalse(scanningApp.$$('#toast').open);
          // Click the Scan button and the scan will fail to start.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          assertTrue(scanningApp.$$('#toast').open);
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#toastInfoIcon'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#getHelpLink'))));
          assertEquals(
              scanningApp.i18n('startScanFailedToast'),
              scanningApp.$$('#toastText').textContent.trim());

          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
        });
  });

  test('PanelContainerContent', () => {
    return initializeScanningApp(expectedScanners, capabilities).then(() => {
      const panelContainer = scanningApp.$$('#panelContainer');
      assertTrue(!!panelContainer);

      const leftPanel = scanningApp.$$('#panelContainer > #leftPanel');
      const rightPanel = scanningApp.$$('#panelContainer > #rightPanel');

      assertTrue(!!leftPanel);
      assertTrue(!!rightPanel);
    });
  });

  test('MoreSettingsToggle', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          // Verify that expandable section is closed by default.
          assertFalse(isSettingsOpen());
          // Expand more settings section.
          return clickMoreSettingsButton();
        })
        .then(() => {
          assertTrue(isSettingsOpen());
          // Close more settings section.
          return clickMoreSettingsButton();
        })
        .then(() => {
          assertFalse(isSettingsOpen());
        });
  });

  test('NoScanners', () => {
    return initializeScanningApp(/*scanners=*/[], /*capabilities=*/ new Map())
        .then(() => {
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#panelContainer'))));
        });
  });

  test('RetryClickLoadsScanners', () => {
    return initializeScanningApp(/*scanners=*/[], /*capabilities=*/ new Map())
        .then(() => {
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#panelContainer'))));

          fakeScanService_.setScanners(expectedScanners);
          fakeScanService_.setCapabilities(capabilities);
          scanningApp.$$('loading-page').$$('#retryButton').click();
          return fakeScanService_.whenCalled('getScanners');
        })
        .then(() => {
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#panelContainer'))));
        });
  });

  test('RecordNoSettingChanges', () => {
    testBrowserProxy.setExpectedNumScanSettingChanges(0);
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
        });
  });

  test('RecordSomeSettingChanges', () => {
    testBrowserProxy.setExpectedNumScanSettingChanges(2);
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#fileTypeSelect').$$('select'),
              FileType.JPG.toString(), /* selectedIndex */ null);
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#resolutionSelect').$$('select'), '75',
              /* selectedIndex */ null);
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
        });
  });

  test('RecordSettingsWithScannerChange', () => {
    testBrowserProxy.setExpectedNumScanSettingChanges(3);
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#colorModeSelect').$$('select'),
              ColorMode.BLACK_AND_WHITE.toString(), /* selectedIndex */ null);
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#scannerSelect').$$('select'), /* value */ null,
              /* selectedIndex */ 1);
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#fileTypeSelect').$$('select'),
              FileType.JPG.toString(), /* selectedIndex */ null);
        })
        .then(() => {
          return changeSelect(
              scanningApp.$$('#resolutionSelect').$$('select'), '150',
              /* selectedIndex */ null);
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
        });
  });
}
