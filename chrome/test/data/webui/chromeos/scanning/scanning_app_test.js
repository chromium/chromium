// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/scanning_app.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {setScanServiceForTesting} from 'chrome://scanning/mojo_interface_provider.js';
import {MAX_NUM_SAVED_SCANNERS, ScannerArr, ScannerSetting, ScanSettings, StartMultiPageScanResponse} from 'chrome://scanning/scanning_app_types.js';
import {getColorModeString, getPageSizeString, tokenToString} from 'chrome://scanning/scanning_app_util.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {changeSelect, createScanner, createScannerSource} from './scanning_app_test_utils.js';
import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

const MY_FILES_PATH = '/home/chronos/user/MyFiles';

// An arbitrary date needed to replace |lastScanDate| in saved scan settings.
const LAST_SCAN_DATE = new Date('1/1/2021');

// Scanner sources names.
const ADF_DUPLEX = 'adf_duplex';
const ADF_SIMPLEX = 'adf_simplex';
const PLATEN = 'platen';

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

const firstPageSizes = [PageSize.A4, PageSize.Letter, PageSize.Max];
const firstColorModes = [ColorMode.BLACK_AND_WHITE, ColorMode.COLOR];
const firstResolutions = [75, 100, 300];

const secondPageSizes = [PageSize.A4, PageSize.Max];
const secondColorModes = [ColorMode.BLACK_AND_WHITE, ColorMode.GRAYSCALE];
const secondResolutions = [150, 600];

const thirdPageSizes = [PageSize.Max];
const thirdColorModes = [ColorMode.BLACK_AND_WHITE];
const thirdResolutions = [75, 200];

const firstScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 1});
const firstScannerName = 'Scanner 1';

const secondScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 2});
const secondScannerName = 'Scanner 2';

const firstCapabilities = {
  sources: [
    createScannerSource(
        SourceType.ADF_DUPLEX, ADF_DUPLEX, firstPageSizes, firstColorModes,
        firstResolutions),
    createScannerSource(
        SourceType.FLATBED, PLATEN, secondPageSizes, firstColorModes,
        firstResolutions),
  ],
};

const secondCapabilities = {
  sources: [
    createScannerSource(
        SourceType.ADF_DUPLEX, ADF_DUPLEX, thirdPageSizes, thirdColorModes,
        thirdResolutions),
    createScannerSource(
        SourceType.ADF_SIMPLEX, ADF_SIMPLEX, secondPageSizes, secondColorModes,
        secondResolutions),
  ],
};

/** @implements {ash.scanning.mojom.ScanServiceInterface} */
class FakeScanService {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /** @private {?ash.scanning.mojom.MultiPageScanControllerInterface} */
    this.multiPageScanController_ = null;

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
    this.resolverMap_.set('startMultiPageScan', new PromiseResolver());
    this.resolverMap_.set('cancelScan', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    const method = this.resolverMap_.get(methodName);
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

  /**
   * @param {?ash.scanning.mojom.MultiPageScanControllerInterface} controller
   */
  setMultiPageScanController(controller) {
    this.multiPageScanController_ = controller;
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
   * @param {number} pageNumber Always 1 for Flatbed scans, increments for ADF
   *    scans.
   * @param {number} newPageIndex The index the new page should take in the
   *    objects array.
   * @return {!Promise}
   */
  simulatePageComplete(pageNumber, newPageIndex) {
    this.scanJobObserverRemote_.onPageProgress(pageNumber, 100);
    const fakePageData = [2, 57, 13, 289];
    this.scanJobObserverRemote_.onPageComplete(fakePageData, newPageIndex);
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

  /**
   * @param {!ash.scanning.mojom.ScanResult} scanResult
   * @return {!Promise}
   */
  simulateMultiPageScanFail(scanResult) {
    this.scanJobObserverRemote_.onMultiPageScanFail(scanResult);
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

  /**
   * @param {!mojoBase.mojom.UnguessableToken} scanner_id
   * @param {!ash.scanning.mojom.ScanSettings} settings
   * @param {!ash.scanning.mojom.ScanJobObserverRemote} remote
   * @return {!Promise<StartMultiPageScanResponse>}
   */
  startMultiPageScan(scanner_id, settings, remote) {
    return new Promise(resolve => {
      this.scanJobObserverRemote_ = remote;
      this.methodCalled('startMultiPageScan');
      resolve({
        controller: this.failStartScan_ ? null : this.multiPageScanController_,
      });
    });
  }

  cancelScan() {
    this.methodCalled('cancelScan');
  }
}

/** @implements {ash.scanning.mojom.MultiPageScanControllerInterface} */
class FakeMultiPageScanController {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /** @private {Object} */
    this.$ = {
      close() {},
    };

    /** @private {number} */
    this.pageIndexToRemove_ = -1;

    /** @private {number} */
    this.pageIndexToRescan_ = -1;

    this.resetForTest();
  }

  resetForTest() {
    this.resolverMap_.set('scanNextPage', new PromiseResolver());
    this.resolverMap_.set('completeMultiPageScan', new PromiseResolver());
    this.resolverMap_.set('rescanPage', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    const method = this.resolverMap_.get(methodName);
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

  /**
   * @param {!mojoBase.mojom.UnguessableToken} scannerId
   * @param {!ash.scanning.mojom.ScanSettings} settings
   * @return {!Promise<{success: boolean}>}
   */
  scanNextPage(scannerId, settings) {
    return new Promise(resolve => {
      this.methodCalled('scanNextPage');
      resolve({success: true});
    });
  }

  /** @param {number} pageIndex */
  removePage(pageIndex) {
    this.pageIndexToRemove_ = pageIndex;
  }

  /**
   * @param {!mojoBase.mojom.UnguessableToken} scannerId
   * @param {!ash.scanning.mojom.ScanSettings} settings
   * @param {number} pageIndex
   * @return {!Promise<{success: boolean}>}
   */
  rescanPage(scannerId, settings, pageIndex) {
    this.pageIndexToRescan_ = pageIndex;
    return new Promise(resolve => {
      this.methodCalled('rescanPage');
      resolve({success: true});
    });
  }

  completeMultiPageScan() {
    this.methodCalled('completeMultiPageScan');
  }

  /** @return {number}*/
  getPageIndexToRemove() {
    return this.pageIndexToRemove_;
  }

  /** @return {number}*/
  getPageIndexToRescan() {
    return this.pageIndexToRescan_;
  }
}

suite('scanningAppTest', function() {
  /** @type {?ScanningAppElement} */
  let scanningApp = null;

  /** @type {?FakeScanService} */
  let fakeScanService_ = null;

  /** @type {?FakeMultiPageScanController} */
  let fakeMultiPageScanController_ = null;

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

  /** @type {?HTMLLinkElement} */
  let linkEl = null;

  const disabledUrl = 'chrome://resources/chromeos/colors/cros_styles.css';

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
    createScanner(secondScannerId, secondScannerName),
  ];

  suiteSetup(() => {
    fakeScanService_ = new FakeScanService();
    setScanServiceForTesting(fakeScanService_);
    fakeMultiPageScanController_ = new FakeMultiPageScanController();
    testBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.instance_ = testBrowserProxy;
    testBrowserProxy.setMyFilesPath(MY_FILES_PATH);
  });

  setup(function() {
    document.body.innerHTML = '';
    linkEl = /**@type {HTMLLinkElement}*/ (document.createElement('link'));
    linkEl.href = disabledUrl;
    document.head.appendChild(linkEl);
  });

  teardown(function() {
    fakeScanService_.resetForTest();
    if (scanningApp) {
      scanningApp.remove();
    }
    if (linkEl) {
      document.head.removeChild(linkEl);
      linkEl = null;
    }
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
    testBrowserProxy.reset();
  });

  /**
   * @param {!ScannerArr} scanners
   * @param {!Map<!mojoBase.mojom.UnguessableToken,
   *     !ash.scanning.mojom.ScannerCapabilities>} capabilities
   * @return {!Promise}
   */
  function initializeScanningApp(scanners, capabilities) {
    fakeScanService_.setMultiPageScanController(fakeMultiPageScanController_);
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
  function clickScanFailedDialogOkButton() {
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

  /**
   * Fetches capabilities then waits for app to change to READY state.
   * @return {!Promise}
   */
  function getScannerCapabilities() {
    return fakeScanService_.whenCalled('getScannerCapabilities')
        .then(() => {
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          // Need to wait for the app to render again when the Source type is
          // selected from saved scan settings.
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        });
  }

  /**
   * Deep equals two ScanSettings objects.
   * @param {!ScanSettings} expectedScanSettings
   * @param {!ScanSettings} actualScanSettings
   */
  function compareSavedScanSettings(expectedScanSettings, actualScanSettings) {
    assertEquals(
        expectedScanSettings.lastUsedScannerName,
        actualScanSettings.lastUsedScannerName);
    assertEquals(
        expectedScanSettings.scanToPath, actualScanSettings.scanToPath);

    // Replace |lastScanDate|, which is a current date timestamp, with a fixed
    // date so assertArrayEquals() can be used.
    expectedScanSettings.scanners.forEach(
        scanner => scanner.lastScanDate = LAST_SCAN_DATE);
    actualScanSettings.scanners.forEach(
        scanner => scanner.lastScanDate = LAST_SCAN_DATE);
    assertArrayEquals(
        expectedScanSettings.scanners, actualScanSettings.scanners);
  }

  // Verify a full scan job can be completed.
  test('Scan', () => {
    /** @type {!Array<!mojoBase.mojom.FilePath>} */
    const scannedFilePaths =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];
    let newPageIndex = 0;

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
          return getScannerCapabilities();
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
              firstCapabilities.sources[1].pageSizes[0].toString(),
              scanningApp.selectedPageSize);
          assertEquals(
              firstCapabilities.sources[1].resolutions[0].toString(),
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
          assertEquals('Scan', scanButton.textContent.trim());
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
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
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
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 2, newPageIndex++);
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

  // Verify the scan failed dialog shows when a scan job fails.
  test('ScanFailed', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          return getScannerCapabilities();
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
          return clickScanFailedDialogOkButton();
        })
        .then(() => {
          // After the dialog closes, the scan button should be enabled and
          // ready to start a new scan.
          assertFalse(scanningApp.$$('#scanFailedDialog').open);
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
        });
  });

  // Verify the scan failed dialog closes and resets the scan app state when the
  // user clicks ESC.
  test('EscClosesScanFailedDialog', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          return getScannerCapabilities();
        })
        .then(() => {
          // Click the Scan button and wait till the scan is started.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        })
        .then(() => {
          // Simulate a progress update.
          return fakeScanService_.simulateProgress(
              /*pageNumber=*/ 1, /*progressPercent=*/ 17);
        })
        .then(() => {
          // Simulate the scan failing.
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kIoError, []);
        })
        .then(() => {
          const scanFailedDialog = scanningApp.$$('#scanFailedDialog');

          // The scan failed dialog should open.
          assertTrue(scanFailedDialog.open);

          // Simulate the ESC key by sending the `cancel` event to the native
          // dialog.
          scanFailedDialog.shadowRoot.querySelector('#dialog').dispatchEvent(
              new Event('cancel'));
          assertFalse(scanningApp.$$('#scanFailedDialog').open);
          assertFalse(scanButton.disabled);
          assertTrue(isVisible(/** @type {!CrButtonElement} */ (scanButton)));
        });
  });

  // Verify a multi-page scan job can be initiated.
  test('MultiPageScan', () => {
    /** @type {!Array<!mojoBase.mojom.FilePath>} */
    const scannedFilePaths = [{'path': '/test/path/scan1.pdf'}];
    let newPageIndex = 0;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
        })
        .then(() => {
          const scanButton = scanningApp.$$('#scanButton');
          assertEquals('Scan page 1', scanButton.textContent.trim());
          scanButton.click();
          return fakeScanService_.whenCalled('startMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          // The scanned images and multi-page scan page should be visible.
          assertTrue(isVisible(/** @type {!HTMLElement} */ (
              scanningApp.$$('#scanPreview').$$('#scannedImages'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('multi-page-scan'))));

          const scanNextPageButton =
              scanningApp.$$('multi-page-scan').$$('#scanButton');
          assertEquals('Scan page 2', scanNextPageButton.textContent.trim());
          scanNextPageButton.click();
          return fakeMultiPageScanController_.whenCalled('scanNextPage');
        })
        .then(() => {
          // Cancel button should be visible while scanning.
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('multi-page-scan').$$('#scanButton'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('multi-page-scan').$$('#cancelButton'))));

          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          // The scanned images and multi-page scan page should still be visible
          // after scanning the next page.
          assertTrue(isVisible(/** @type {!HTMLElement} */ (
              scanningApp.$$('#scanPreview').$$('#scannedImages'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('multi-page-scan'))));

          const scanNextPageButton =
              scanningApp.$$('multi-page-scan').$$('#scanButton');
          assertEquals('Scan page 3', scanNextPageButton.textContent.trim());

          scanningApp.$$('multi-page-scan').$$('#saveButton').click();
          return fakeMultiPageScanController_.whenCalled(
              'completeMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kSuccess, scannedFilePaths);
        })
        .then(() => {
          scannedImages = scanningApp.$$('#scanPreview').$$('#scannedImages');
          assertTrue(isVisible(/** @type {!HTMLElement} */ (scannedImages)));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));
          assertArrayEquals(
              scannedFilePaths,
              scanningApp.$$('scan-done-section').scannedFilePaths);
        });
  });

  // Verify a multi-page scan job can fail scanning a page then scan another
  // page successfully.
  test('MultiPageScanPageFailed', () => {
    let newPageIndex = 0;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          return fakeScanService_.whenCalled('startMultiPageScan');
        })
        .then(() => {
          assertEquals(
              'Scanning page 1',
              scanningApp.$$('#scanPreview')
                  .$$('#progressText')
                  .textContent.trim());
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          scanningApp.$$('multi-page-scan').$$('#scanButton').click();
          return fakeMultiPageScanController_.whenCalled('scanNextPage');
        })
        .then(() => {
          assertEquals(
              'Scanning page 2',
              scanningApp.$$('#scanPreview')
                  .$$('#progressText')
                  .textContent.trim());
          return fakeScanService_.simulateMultiPageScanFail(
              ash.scanning.mojom.ScanResult.kFlatbedOpen);
        })
        .then(() => {
          // The scan failed dialog should open.
          assertTrue(scanningApp.$$('#scanFailedDialog').open);
          assertEquals(
              loadTimeData.getString('scanFailedDialogFlatbedOpenText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());

          // Click the dialog's Ok button to return to MULTI_PAGE_NEXT_ACTION
          // state.
          return clickScanFailedDialogOkButton();
        })
        .then(() => {
          // After the dialog closes, the scan next page button should still
          // say 'Scan Page 2'.
          const scanNextPageButton =
              scanningApp.$$('multi-page-scan').$$('#scanButton');
          assertEquals('Scan page 2', scanNextPageButton.textContent.trim());
          scanNextPageButton.click();
          return fakeMultiPageScanController_.whenCalled('scanNextPage');
        })
        .then(() => {
          assertEquals(
              'Scanning page 2',
              scanningApp.$$('#scanPreview')
                  .$$('#progressText')
                  .textContent.trim());
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          scanningApp.$$('multi-page-scan').$$('#saveButton').click();
          return fakeMultiPageScanController_.whenCalled(
              'completeMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kSuccess,
              [{'path': '/test/path/scan1.pdf'}]);
        })
        .then(() => {
          scannedImages = scanningApp.$$('#scanPreview').$$('#scannedImages');

          // There should be 2 images from scanning once, failing once, then
          // scanning again successfully.
          assertEquals(2, scannedImages.querySelectorAll('img').length);
        });
  });

  // Verify a scan can be canceled during a multi-page scan session.
  test('MultiPageCancelScan', () => {
    let newPageIndex = 0;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return flushTasks();
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
          scanningApp.$$('#scanButton').click();
          return fakeScanService_.whenCalled('startMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          scanningApp.$$('multi-page-scan').$$('#scanButton').click();
          return fakeMultiPageScanController_.whenCalled('scanNextPage');
        })
        .then(() => {
          // Click the Cancel button to cancel the scan.
          scanningApp.$$('multi-page-scan').$$('#cancelButton').click();
          return fakeScanService_.whenCalled('cancelScan');
        })
        .then(() => {
          // Cancel button should be disabled while canceling is in progress.
          assertTrue(
              scanningApp.$$('multi-page-scan').$$('#cancelButton').disabled);

          // Simulate cancel completing successfully.
          return fakeScanService_.simulateCancelComplete(true);
        })
        .then(() => {
          // After canceling is complete, the Scan Next Page button should be
          // visible and showing the correct page number to scan. The cancel
          // button should be hidden.
          const scanNextPageButton =
              scanningApp.$$('multi-page-scan').$$('#scanButton');
          assertTrue(
              isVisible(/** @type {!CrButtonElement} */ (scanNextPageButton)));
          assertEquals('Scan page 2', scanNextPageButton.textContent.trim());
          assertFalse(isVisible(/** @type {!CrButtonElement} */ (
              scanningApp.$$('multi-page-scan').$$('#cancelButton'))));
          assertTrue(scanningApp.$$('#toast').open);
        });
  });

  // Verify the correct page can be removed from a multi-page scan job by
  // scanning three pages then removing the second page.
  test('MultiPageScanPageRemoved', () => {
    const pageIndexToRemove = 1;
    let expectedObjectUrls;
    let newPageIndex = 0;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          return fakeScanService_.whenCalled('startMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          scanningApp.$$('multi-page-scan').$$('#scanButton').click();
          return fakeMultiPageScanController_.whenCalled('scanNextPage');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          scanningApp.$$('multi-page-scan').$$('#scanButton').click();
          return fakeMultiPageScanController_.whenCalled('scanNextPage');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          // Save the current scanned images
          expectedObjectUrls = scanningApp.$$('#scanPreview').objectUrls;
          assertEquals(3, expectedObjectUrls.length);

          // Open the remove page dialog.
          scanningApp.$$('#scanPreview')
              .$$('action-toolbar')
              .dispatchEvent(new CustomEvent(
                  'show-remove-page-dialog', {detail: pageIndexToRemove}));
          return flushTasks();
        })
        .then(() => {
          scanningApp.$$('#scanPreview').$$('#actionButton').click();
          return flushTasks();
        })
        .then(() => {
          assertEquals(
              pageIndexToRemove,
              fakeMultiPageScanController_.getPageIndexToRemove());

          // Remove the second page from the expected scanned images and verify
          // the correct image was removed from the actual scanned images.
          expectedObjectUrls.splice(pageIndexToRemove, 1);
          assertArrayEquals(
              expectedObjectUrls, scanningApp.$$('#scanPreview').objectUrls);
        });
  });

  // Verify if there's only one page in the multi-page scan session it can be
  // removed, the scan is reset, and the user is returned to the scan settings
  // page.
  test('MultiPageScanRemoveLastPage', () => {
    let newPageIndex = 0;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          return fakeScanService_.whenCalled('startMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          // Open the remove page dialog.
          scanningApp.$$('#scanPreview')
              .$$('action-toolbar')
              .dispatchEvent(
                  new CustomEvent('show-remove-page-dialog', {detail: 0}));
          return flushTasks();
        })
        .then(() => {
          scanningApp.$$('#scanPreview').$$('#actionButton').click();
          return flushTasks();
        })
        .then(() => {
          assertArrayEquals([], scanningApp.$$('#scanPreview').objectUrls);
          --newPageIndex;

          // Attempt a new multi-page scan from the scan settings page.
          scanningApp.$$('#scanButton').click();
          return fakeScanService_.whenCalled('startMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          // The scanned images and multi-page scan page should be visible.
          assertTrue(isVisible(/** @type {!HTMLElement} */ (
              scanningApp.$$('#scanPreview').$$('#scannedImages'))));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('multi-page-scan'))));

          const scanNextPageButton =
              scanningApp.$$('multi-page-scan').$$('#scanButton');
          assertEquals('Scan page 2', scanNextPageButton.textContent.trim());
        });
  });

  // Verify one page can be scanned and then rescanned in a multi-page scan job.
  test('MultiPageScanRescanOnePage', () => {
    /** @type {!Array<!mojoBase.mojom.FilePath>} */
    const scannedFilePaths = [{'path': '/test/path/scan1.pdf'}];
    const pageIndexToRescan = 0;

    let scanPreview;
    let expectedObjectUrls;
    let newPageIndex = 0;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanPreview = scanningApp.$$('#scanPreview');
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          return fakeScanService_.whenCalled('startMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          // Save the current scanned image.
          expectedObjectUrls = [...scanPreview.objectUrls];
          assertEquals(1, expectedObjectUrls.length);

          // Open the rescan page dialog.
          scanPreview.$$('action-toolbar')
              .dispatchEvent(new CustomEvent(
                  'show-rescan-page-dialog', {detail: pageIndexToRescan}));
          return flushTasks();
        })
        .then(() => {
          // Verify the dialog shows we are rescanning the correct page number.
          assertEquals(
              'Rescan page?',
              scanPreview.$$('#dialogTitle').textContent.trim());

          scanPreview.$$('#actionButton').click();
          return fakeMultiPageScanController_.whenCalled('rescanPage');
        })
        .then(() => {
          // Verify the progress text shows we are attempting to rescan the
          // first page.
          progressText = scanPreview.$$('#progressText');
          assertEquals('Scanning page 1', progressText.textContent.trim());
          assertEquals(
              pageIndexToRescan,
              fakeMultiPageScanController_.getPageIndexToRescan());
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, /*newPageIndex=*/ 0);
        })
        .then(() => {
          // After rescanning verify the page is different.
          const actualObjectUrls = scanPreview.objectUrls;
          assertEquals(1, actualObjectUrls.length);
          assertNotEquals(expectedObjectUrls[0], actualObjectUrls[0]);
        })
        .then(() => {
          // Save the one page scan.
          scanningApp.$$('multi-page-scan').$$('#saveButton').click();
          return fakeMultiPageScanController_.whenCalled(
              'completeMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulateScanComplete(
              ash.scanning.mojom.ScanResult.kSuccess, scannedFilePaths);
        })
        .then(() => {
          scannedImages = scanningApp.$$('#scanPreview').$$('#scannedImages');
          assertTrue(isVisible(/** @type {!HTMLElement} */ (scannedImages)));
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('scan-done-section'))));
          assertArrayEquals(
              scannedFilePaths,
              scanningApp.$$('scan-done-section').scannedFilePaths);
        });
  });

  // Verify a page can be rescanned in a multi-page scan job. This test
  // simulates scanning two pages, rescanning the first page, then scanning a
  // third page.
  test('MultiPageScanPageRescanned', () => {
    const pageIndexToRescan = 0;

    let scanPreview;
    let expectedObjectUrls;
    let newPageIndex = 0;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanPreview = scanningApp.$$('#scanPreview');
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          return fakeScanService_.whenCalled('startMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          scanningApp.$$('multi-page-scan').$$('#scanButton').click();
          return fakeMultiPageScanController_.whenCalled('scanNextPage');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          // Save the current scanned images.
          expectedObjectUrls = [...scanPreview.objectUrls];
          assertEquals(2, expectedObjectUrls.length);

          // Open the rescan page dialog.
          scanPreview.$$('action-toolbar')
              .dispatchEvent(new CustomEvent(
                  'show-rescan-page-dialog', {detail: pageIndexToRescan}));
          return flushTasks();
        })
        .then(() => {
          // Verify the dialog shows we are rescanning the correct page number.
          assertEquals(
              'Rescan page 1?',
              scanPreview.$$('#dialogTitle').textContent.trim());

          scanPreview.$$('#actionButton').click();
          return fakeMultiPageScanController_.whenCalled('rescanPage');
        })
        .then(() => {
          // Verify the progress text shows we are attempting to rescan the
          // first page.
          progressText = scanPreview.$$('#progressText');
          assertEquals('Scanning page 1', progressText.textContent.trim());
          assertEquals(
              pageIndexToRescan,
              fakeMultiPageScanController_.getPageIndexToRescan());
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, /*newPageIndex=*/ 0);
        })
        .then(() => {
          // After rescanning verify that the first page changed but the second
          // page stayed the same.
          const actualObjectUrls = scanPreview.objectUrls;
          assertEquals(2, actualObjectUrls.length);
          assertNotEquals(expectedObjectUrls[0], actualObjectUrls[0]);
          assertEquals(expectedObjectUrls[1], actualObjectUrls[1]);
        })
        .then(() => {
          // Verify that after rescanning, the scan button shows the correct
          // next page number to scan.
          const scanButton =
              scanningApp.$$('multi-page-scan').$$('#scanButton');
          assertEquals('Scan page 3', scanButton.textContent.trim());

          scanButton.click();
          return fakeMultiPageScanController_.whenCalled('scanNextPage');
        })
        .then(() => {
          // Verify the progress text shows we are scanning the third page.
          assertEquals('Scanning page 3', progressText.textContent.trim());
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          assertEquals(3, scanPreview.objectUrls.length);
        });
  });

  // Verify that if rescanning a page fails, the page numbers update correctly.
  test('MultiPageScanPageRescanFail', () => {
    const pageIndexToRescan = 0;
    let scanPreview;
    let expectedObjectUrls;
    let newPageIndex = 0;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanPreview = scanningApp.$$('#scanPreview');
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          return fakeScanService_.whenCalled('startMultiPageScan');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          scanningApp.$$('multi-page-scan').$$('#scanButton').click();
          return fakeMultiPageScanController_.whenCalled('scanNextPage');
        })
        .then(() => {
          return fakeScanService_.simulatePageComplete(
              /*pageNumber=*/ 1, newPageIndex++);
        })
        .then(() => {
          // Save the current scanned images.
          expectedObjectUrls = [...scanPreview.objectUrls];
          assertEquals(2, expectedObjectUrls.length);

          // Open the rescan page dialog.
          scanPreview.$$('action-toolbar')
              .dispatchEvent(new CustomEvent(
                  'show-rescan-page-dialog', {detail: pageIndexToRescan}));
          return flushTasks();
        })
        .then(() => {
          scanPreview.$$('#actionButton').click();
          return fakeMultiPageScanController_.whenCalled('rescanPage');
        })
        .then(() => {
          return fakeScanService_.simulateMultiPageScanFail(
              ash.scanning.mojom.ScanResult.kFlatbedOpen);
        })
        .then(() => {
          // The scan failed dialog should open.
          assertTrue(scanningApp.$$('#scanFailedDialog').open);
          assertEquals(
              loadTimeData.getString('scanFailedDialogFlatbedOpenText'),
              scanningApp.$$('#scanFailedDialogText').textContent.trim());

          // Click the dialog's Ok button to return to MULTI_PAGE_NEXT_ACTION
          // state.
          return clickScanFailedDialogOkButton();
        })
        .then(() => {
          // Verify that the pages stayed the same.
          const actualObjectUrls = scanPreview.objectUrls;
          assertEquals(2, actualObjectUrls.length);
          assertArrayEquals(expectedObjectUrls, actualObjectUrls);

          // Verify the scan button shows the correct next page number to scan.
          assertEquals(
              'Scan page 3',
              scanningApp.$$('multi-page-scan')
                  .$$('#scanButton')
                  .textContent.trim());
        });
  });

  // Verify the page size, color, and resolution dropdowns contain the correct
  // elements when each source is selected.
  test('SourceChangeUpdatesDropdowns', () => {
    return initializeScanningApp(expectedScanners.slice(1), capabilities)
        .then(() => {
          sourceSelect = scanningApp.$$('#sourceSelect').$$('select');
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(2, sourceSelect.length);
          return changeSelect(
              /** @type {!HTMLSelectElement} */ (sourceSelect),
              /* value=*/ null, /* selectedIndex=*/ 0);
        })
        .then(() => {
          colorModeSelect = scanningApp.$$('#colorModeSelect').$$('select');
          pageSizeSelect = scanningApp.$$('#pageSizeSelect').$$('select');
          resolutionSelect = scanningApp.$$('#resolutionSelect').$$('select');

          assertEquals(2, colorModeSelect.length);
          assertEquals(
              getColorModeString(secondColorModes[0]),
              colorModeSelect.options[0].textContent.trim());
          assertEquals(
              getColorModeString(secondColorModes[1]),
              colorModeSelect.options[1].textContent.trim());
          assertEquals(2, pageSizeSelect.length);
          assertEquals(
              getPageSizeString(secondPageSizes[0]),
              pageSizeSelect.options[0].textContent.trim());
          assertEquals(
              getPageSizeString(secondPageSizes[1]),
              pageSizeSelect.options[1].textContent.trim());
          assertEquals(2, resolutionSelect.length);
          assertEquals(
              secondResolutions[0].toString() + ' dpi',
              resolutionSelect.options[0].textContent.trim());
          assertEquals(
              secondResolutions[1].toString() + ' dpi',
              resolutionSelect.options[1].textContent.trim());
          return changeSelect(
              /** @type {!HTMLSelectElement} */ (sourceSelect),
              /* value=*/ null, /* selectedIndex=*/ 1);
        })
        .then(() => {
          assertEquals(1, colorModeSelect.length);
          assertEquals(
              getColorModeString(thirdColorModes[0]),
              colorModeSelect.options[0].textContent.trim());
          assertEquals(1, pageSizeSelect.length);
          assertEquals(
              getPageSizeString(thirdPageSizes[0]),
              pageSizeSelect.options[0].textContent.trim());
          assertEquals(2, resolutionSelect.length);
          assertEquals(
              thirdResolutions[0].toString() + ' dpi',
              resolutionSelect.options[0].textContent.trim());
          assertEquals(
              thirdResolutions[1].toString() + ' dpi',
              resolutionSelect.options[1].textContent.trim());
        });
  });

  // Verify the correct message is shown in the scan failed dialog based on the
  // error type.
  test('ScanResults', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
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
          return clickScanFailedDialogOkButton();
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
          return clickScanFailedDialogOkButton();
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
          return clickScanFailedDialogOkButton();
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
          return clickScanFailedDialogOkButton();
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
          return clickScanFailedDialogOkButton();
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
          return clickScanFailedDialogOkButton();
        });
  });

  // Verify a scan job can be canceled.
  test('CancelScan', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          cancelButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#cancelButton'));
          return getScannerCapabilities();
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

  // Verify the cancel scan failed dialog shows when a scan job fails to cancel.
  test('CancelScanFailed', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          cancelButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#cancelButton'));
          return getScannerCapabilities();
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

  // Verify the scan failed to start toast shows when a scan job fails to start.
  test('ScanFailedToStart', () => {
    fakeScanService_.setFailStartScan(true);

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          scanButton =
              /** @type {!CrButtonElement} */ (scanningApp.$$('#scanButton'));
          return getScannerCapabilities();
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

  // Verify the left and right panel exist on app initialization.
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

  // Verify the more settings toggle behavior.
  test('MoreSettingsToggle', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
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

  // Verify the loading page container is shown when no scanners are available.
  test('NoScanners', () => {
    return initializeScanningApp(/*scanners=*/[], /*capabilities=*/ new Map())
        .then(() => {
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('loading-page'))));
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (scanningApp.$$('#panelContainer'))));
        });
  });

  // Verify clicking the retry button loads available scanners.
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

  // Verify no changes are recorded when a scan job is initiated without any
  // settings changes.
  test('RecordNoSettingChanges', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();
          const numScanSettingChanges =
              testBrowserProxy.getArgs('recordNumScanSettingChanges')[0];
          assertEquals(0, numScanSettingChanges);
        });
  });

  // Verify the correct amount of settings changes are recorded when a scan job
  // is initiated.
  test('RecordSomeSettingChanges', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
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
          const numScanSettingChanges =
              testBrowserProxy.getArgs('recordNumScanSettingChanges')[0];
          assertEquals(2, numScanSettingChanges);
        });
  });

  // Verify the correct amount of changes are recorded after the selected
  // scanner is changed then a scan job is initiated.
  test('RecordSettingsWithScannerChange', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
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
          const numScanSettingChanges =
              testBrowserProxy.getArgs('recordNumScanSettingChanges')[0];
          assertEquals(3, numScanSettingChanges);
        });
  });

  // Verify the default scan settings are chosen on app load.
  test('DefaultScanSettings', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              PLATEN, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              loadTimeData.getString('myFilesSelectOption'),
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPdf.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kColor.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kIsoA4.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '300', scanningApp.$$('#resolutionSelect').$$('select').value);
        });
  });

  // Verify the first option in each settings dropdown is selected when the
  // default option is not available on the selected scanner.
  test('DefaultScanSettingsNotAvailable', () => {
    return initializeScanningApp(expectedScanners.slice(1), capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(secondScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              ADF_SIMPLEX, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              loadTimeData.getString('myFilesSelectOption'),
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPdf.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kBlackAndWhite.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kIsoA4.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '600', scanningApp.$$('#resolutionSelect').$$('select').value);
        });
  });

  // Verify the default scan settings are used when saved settings are not
  // available for the selected scanner.
  test('SavedSettingsNotAvailable', () => {
    const savedScanSettings = {
      lastUsedScannerName: 'Wrong Scanner',
      scanToPath: 'scan/to/path',
      scanners: [{
        name: 'Wrong Scanner',
        lastScanDate: new Date(),
        sourceName: ADF_DUPLEX,
        fileType: ash.scanning.mojom.FileType.kPng,
        colorMode: ash.scanning.mojom.ColorMode.kGrayscale,
        pageSize: ash.scanning.mojom.PageSize.kMax,
        resolutionDpi: 100,
        multiPageScanChecked: false,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              PLATEN, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              loadTimeData.getString('myFilesSelectOption'),
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPdf.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kColor.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kIsoA4.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '300', scanningApp.$$('#resolutionSelect').$$('select').value);
          assertFalse(scanningApp.multiPageScanChecked);
        });
  });

  // Verify saved settings are applied when available for the selected scanner.
  test('ApplySavedSettings', () => {
    const selectedPath = {baseName: 'path', filePath: 'valid/scan/to/path'};
    testBrowserProxy.setSavedSettingsSelectedPath(selectedPath);

    const savedScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: selectedPath.filePath,
      scanners: [{
        name: firstScannerName,
        lastScanDate: new Date(),
        sourceName: PLATEN,
        fileType: ash.scanning.mojom.FileType.kPdf,
        colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
        pageSize: ash.scanning.mojom.PageSize.kMax,
        resolutionDpi: 75,
        multiPageScanChecked: true,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              PLATEN, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              selectedPath.baseName,
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPdf.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kBlackAndWhite.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kMax.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '75', scanningApp.$$('#resolutionSelect').$$('select').value);
          assertTrue(scanningApp.multiPageScanChecked);
        });
  });

  // Verify if the setting value stored in saved settings is no longer
  // available on the selected scanner, the default setting is chosen.
  test('SettingNotFoundInCapabilities', () => {
    const selectedPath = {baseName: 'path', filePath: 'valid/scan/to/path'};
    testBrowserProxy.setSavedSettingsSelectedPath(selectedPath);

    const savedScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: 'this/path/does/not/exist',
      scanners: [{
        name: firstScannerName,
        lastScanDate: new Date(),
        sourceName: ADF_SIMPLEX,
        fileType: -1,
        colorMode: ash.scanning.mojom.ColorMode.kGrayscale,
        pageSize: -1,
        resolutionDpi: 600,
        multiPageScanChecked: false,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
          assertEquals(
              PLATEN, scanningApp.$$('#sourceSelect').$$('select').value);
          assertEquals(
              loadTimeData.getString('myFilesSelectOption'),
              scanningApp.$$('#scanToSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.FileType.kPdf.toString(),
              scanningApp.$$('#fileTypeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.ColorMode.kColor.toString(),
              scanningApp.$$('#colorModeSelect').$$('select').value);
          assertEquals(
              ash.scanning.mojom.PageSize.kIsoA4.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          assertEquals(
              '300', scanningApp.$$('#resolutionSelect').$$('select').value);
          assertFalse(scanningApp.multiPageScanChecked);
        });
  });

  // Verify if |multiPageScanChecked| is true in saved settings but the
  // scanner's capabilities doesn't support it, the multi-page scan checkbox
  // will not be set.
  test('MultiPageNotAvailableFromCapabilities', () => {
    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: '',
      scanners: [{
        name: secondScannerName,
        lastScanDate: new Date(),
        sourceName: PLATEN,
        fileType: ash.scanning.mojom.FileType.kPdf,
        colorMode: ash.scanning.mojom.ColorMode.kGrayscale,
        pageSize: ash.scanning.mojom.PageSize.kNaLetter,
        resolutionDpi: 600,
        multiPageScanChecked: true,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          // `secondScanner` does not have PLATEN in it's capabilities so the
          // multi-page scan checkbox should not get set.
          assertFalse(scanningApp.multiPageScanChecked);
        });
  });

  // Verify if the |multiPageScanChecked| is not present in the saved settings
  // JSON (i.e. the first time the feature is enabled), the multi-page scan
  // checkbox will not be set.
  test('MultiPageNotInSavedSettings', () => {
    const savedScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: '',
      scanners: [{
        name: secondScannerName,
        lastScanDate: new Date(),
        sourceName: PLATEN,
        fileType: ash.scanning.mojom.FileType.kPdf,
        colorMode: ash.scanning.mojom.ColorMode.kGrayscale,
        pageSize: ash.scanning.mojom.PageSize.kNaLetter,
        resolutionDpi: 600,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          // The multi-page scan checkbox should not get set because it wasn't
          // present in the saved settings.
          assertFalse(scanningApp.multiPageScanChecked);
        });
  });

  // Verify the last used scanner is selected from saved settings.
  test('selectLastUsedScanner', () => {
    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: 'scan/to/path',
      scanners: [{
        name: secondScannerName,
        lastScanDate: new Date(),
        sourceName: ADF_DUPLEX,
        fileType: ash.scanning.mojom.FileType.kPng,
        colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
        pageSize: ash.scanning.mojom.PageSize.kMax,
        resolutionDpi: 75,
        multiPageScanChecked: false,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          assertEquals(
              tokenToString(secondScannerId),
              scanningApp.$$('#scannerSelect').$$('select').value);
        });
  });

  // Verify the scan settings are sent to the Pref service to be saved.
  test('saveScanSettings', () => {
    const scannerSetting = {
      name: secondScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_DUPLEX,
      fileType: ash.scanning.mojom.FileType.kPng,
      colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
      pageSize: ash.scanning.mojom.PageSize.kMax,
      resolutionDpi: 100,
      multiPageScanChecked: false,
    };

    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: [scannerSetting],
    };

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedScannerId = tokenToString(secondScannerId);
          scanningApp.selectedSource = scannerSetting.sourceName;
          scanningApp.selectedFileType = scannerSetting.fileType.toString();
          scanningApp.selectedColorMode = scannerSetting.colorMode.toString();
          scanningApp.selectedPageSize = scannerSetting.pageSize.toString();
          scanningApp.selectedResolution =
              scannerSetting.resolutionDpi.toString();

          scanningApp.$$('#scanButton').click();

          const actualSavedScanSettings = /** @type {!ScanSettings} */
              (JSON.parse(/** @type {string} */ (
                  testBrowserProxy.getArgs('saveScanSettings')[0])));
          compareSavedScanSettings(savedScanSettings, actualSavedScanSettings);
        });
  });

  // Verify that the correct scanner setting is replaced when saving scan
  // settings to the Pref service.
  test('replaceExistingScannerInScanSettings', () => {
    const firstScannerSetting = {
      name: firstScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_DUPLEX,
      fileType: ash.scanning.mojom.FileType.kPng,
      colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
      pageSize: ash.scanning.mojom.PageSize.kMax,
      resolutionDpi: 100,
      multiPageScanChecked: false,
    };

    // The saved scan settings for the second scanner. This is loaded from the
    // Pref service when Scan app is initialized and sets the initial scan
    // settings.
    const initialSecondScannerSetting = {
      name: secondScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_DUPLEX,
      fileType: ash.scanning.mojom.FileType.kPng,
      colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
      pageSize: ash.scanning.mojom.PageSize.kMax,
      resolutionDpi: 100,
      multiPageScanChecked: false,
    };

    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: [firstScannerSetting, initialSecondScannerSetting],
    };

    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    // The new scan settings for the second scanner that will replace the
    // initial scan settings.
    const newSecondScannerSetting = {
      name: secondScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_SIMPLEX,
      fileType: ash.scanning.mojom.FileType.kJpg,
      colorMode: ash.scanning.mojom.ColorMode.kGrayscale,
      pageSize: ash.scanning.mojom.PageSize.kIsoA4,
      resolutionDpi: 600,
      multiPageScanChecked: false,
    };
    savedScanSettings.scanners[1] = newSecondScannerSetting;

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedScannerId = tokenToString(secondScannerId);
          scanningApp.selectedSource = newSecondScannerSetting.sourceName;
          scanningApp.selectedFileType =
              newSecondScannerSetting.fileType.toString();
          scanningApp.selectedColorMode =
              newSecondScannerSetting.colorMode.toString();
          scanningApp.selectedPageSize =
              newSecondScannerSetting.pageSize.toString();
          scanningApp.selectedResolution =
              newSecondScannerSetting.resolutionDpi.toString();

          scanningApp.$$('#scanButton').click();

          const actualSavedScanSettings = /** @type {!ScanSettings} */
              (JSON.parse(/** @type {string} */ (
                  testBrowserProxy.getArgs('saveScanSettings')[0])));
          compareSavedScanSettings(savedScanSettings, actualSavedScanSettings);
        });
  });

  // Verify that the correct scanner gets evicted when there are too many
  // scanners in saved scan settings.
  test('evictScannersOverTheMaxLimit', () => {
    const scannerToEvict = {
      name: secondScannerName,
      lastScanDate: '1/1/2021',
      sourceName: ADF_DUPLEX,
      fileType: ash.scanning.mojom.FileType.kPng,
      colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
      pageSize: ash.scanning.mojom.PageSize.kMax,
      resolutionDpi: 100,
      multiPageScanChecked: false,
    };

    // Create an identical scanner with `lastScanDate` set to infinity so it
    // will always have a later `lastScanDate` than |scannerToEvict|.
    const scannerToKeep = Object.assign({}, scannerToEvict);
    scannerToKeep.lastScanDate = '9999-12-31T08:00:00.000Z';
    assertTrue(
        new Date(scannerToKeep.lastScanDate) >
        new Date(scannerToEvict.lastScanDate));

    /** @type {!Array<!ScannerSetting>} */
    const scannersToKeep =
        new Array(MAX_NUM_SAVED_SCANNERS).fill(scannerToKeep);

    // Put |scannerToEvict| in the front of |scannersToKeep| to test that it
    // get correctly sorted to the back of the array when evicting scanners.
    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: [scannerToEvict].concat(scannersToKeep),
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();

          const actualSavedScanSettings = /** @type {!ScanSettings} */
              (JSON.parse(/** @type {string} */ (
                  testBrowserProxy.getArgs('saveScanSettings')[0])));
          assertEquals(
              MAX_NUM_SAVED_SCANNERS, actualSavedScanSettings.scanners.length);
          assertArrayEquals(scannersToKeep, actualSavedScanSettings.scanners);
        });
  });

  // Verify that no scanners get evicted when the number of scanners in saved
  // scan settings is equal to |MAX_NUM_SAVED_SCANNERS|.
  test('doNotEvictScannersAtMax', () => {
    /** @type {!Array<!ScannerSetting>} */
    const scanners = new Array(MAX_NUM_SAVED_SCANNERS);
    for (let i = 0; i < MAX_NUM_SAVED_SCANNERS; i++) {
      scanners[i] = {
        name: 'Scanner ' + (i + 1),
        lastScanDate: new Date(new Date().getTime() + i),
        sourceName: ADF_DUPLEX,
        fileType: ash.scanning.mojom.FileType.kPng,
        colorMode: ash.scanning.mojom.ColorMode.kBlackAndWhite,
        pageSize: ash.scanning.mojom.PageSize.kMax,
        resolutionDpi: 300,
        multiPageScanChecked: false,
      };
    }

    const savedScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: scanners,
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.$$('#scanButton').click();

          const actualSavedScanSettings = /** @type {!ScanSettings} */
              (JSON.parse(/** @type {string} */ (
                  testBrowserProxy.getArgs('saveScanSettings')[0])));
          assertEquals(
              MAX_NUM_SAVED_SCANNERS, actualSavedScanSettings.scanners.length);
          compareSavedScanSettings(savedScanSettings, actualSavedScanSettings);
        });
  });

  // Verify that the multi-page scanning checkbox is only visible when both
  // Flatbed and PDF scan settings are selected.
  test('showMultiPageCheckbox', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = ADF_DUPLEX;
          scanningApp.selectedFileType = FileType.PNG.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('multi-page-checkbox').$$('#checkboxDiv'))));

          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PNG.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('multi-page-checkbox').$$('#checkboxDiv'))));

          scanningApp.selectedSource = ADF_DUPLEX;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          assertFalse(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('multi-page-checkbox').$$('#checkboxDiv'))));

          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return waitAfterNextRender(/** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          assertTrue(isVisible(
              /** @type {!HTMLElement} */ (
                  scanningApp.$$('#multiPageCheckbox').$$('#checkboxDiv'))));
        });
  });

  // Verify a normal scan is started when the multi-page checkbox is checked
  // while a non-PDF file type is selected.
  test('OnlyMultiPageScanWhenPDFIsSelected', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return flushTasks();
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
        })
        .then(() => {
          assertEquals(
              'Scan page 1', scanningApp.$$('#scanButton').textContent.trim());

          // Leave the multi-page checkbox checked but switch the file type.
          scanningApp.selectedFileType = FileType.PNG.toString();
          return flushTasks();
        })
        .then(() => {
          const scanButton = scanningApp.$$('#scanButton');
          assertEquals('Scan', scanButton.textContent.trim());

          // When scan button is clicked expect a normal scan to start.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        });
  });

  // Verify a normal scan is started when the multi-page checkbox is checked
  // while a non-Flatbed source type is selected.
  test('OnlyMultiPageScanWhenFlatbedIsSelected', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          scanningApp.selectedFileType = FileType.PDF.toString();
          return flushTasks();
        })
        .then(() => {
          scanningApp.multiPageScanChecked = true;
        })
        .then(() => {
          assertEquals(
              'Scan page 1', scanningApp.$$('#scanButton').textContent.trim());

          // Leave the multi-page checkbox checked but switch the source.
          scanningApp.selectedSource = ADF_SIMPLEX;
          return flushTasks();
        })
        .then(() => {
          const scanButton = scanningApp.$$('#scanButton');
          assertEquals('Scan', scanButton.textContent.trim());

          // When scan button is clicked expect a normal scan to start.
          scanButton.click();
          return fakeScanService_.whenCalled('startScan');
        });
  });

  // Verify the scan settings update according to the source selected.
  test('UpdateSettingsBySource', () => {
    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return getScannerCapabilities();
        })
        .then(() => {
          scanningApp.selectedSource = PLATEN;
          return waitAfterNextRender(
              /** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          const pageSizeSelector =
              scanningApp.$$('#pageSizeSelect').$$('select');
          changeSelect(
              pageSizeSelector, PageSize.A4.toString(),
              /* selectedIndex */ null);
          assertEquals(
              ash.scanning.mojom.PageSize.kIsoA4.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          changeSelect(
              pageSizeSelector, PageSize.Max.toString(),
              /* selectedIndex */ null);
          assertEquals(
              ash.scanning.mojom.PageSize.kMax.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
        })
        .then(() => {
          scanningApp.selectedSource = ADF_DUPLEX;
          return waitAfterNextRender(
              /** @type {!HTMLElement} */ (scanningApp));
        })
        .then(() => {
          const pageSizeSelector =
              scanningApp.$$('#pageSizeSelect').$$('select');
          changeSelect(
              pageSizeSelector, PageSize.A4.toString(),
              /* selectedIndex */ null);
          assertEquals(
              ash.scanning.mojom.PageSize.kIsoA4.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          changeSelect(
              pageSizeSelector, PageSize.Letter.toString(),
              /* selectedIndex */ null);
          assertEquals(
              ash.scanning.mojom.PageSize.kNaLetter.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
          changeSelect(
              pageSizeSelector, PageSize.Max.toString(),
              /* selectedIndex */ null);
          assertEquals(
              ash.scanning.mojom.PageSize.kMax.toString(),
              scanningApp.$$('#pageSizeSelect').$$('select').value);
        });
  });

  // Verify cros_styles.css kept when `isJellyEnabledForScanningApp` is false.
  test('IsJellyEnabledForScanningApp_DisabledKeepsCSS', async () => {
    loadTimeData.overrideValues({
      isJellyEnabledForScanningApp: false,
    });
    await initializeScanningApp(expectedScanners, capabilities);

    assertTrue(linkEl.href.includes(disabledUrl));
  });

  // Verify cros_styles.css replaced when `isJellyEnabledForScanningApp` is
  // true.
  test('IsJellyEnabledForScanningApp_EnabledUpdateCSS', async () => {
    loadTimeData.overrideValues({
      isJellyEnabledForScanningApp: true,
    });
    await initializeScanningApp(expectedScanners, capabilities);

    assertTrue(linkEl.href.includes('chrome://theme/colors.css'));
  });
});
