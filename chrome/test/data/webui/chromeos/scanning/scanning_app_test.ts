// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/scanning_app.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import type {PaperProgressElement} from 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import {setScanServiceForTesting} from 'chrome://scanning/mojo_interface_provider.js';
import {ScanDoneSectionElement} from 'chrome://scanning/scan_done_section.js';
import {ScanPreviewElement} from 'chrome://scanning/scan_preview.js';
import {ColorMode, FileType, PageSize, ScanResult, SourceType} from 'chrome://scanning/scanning.mojom-webui.js';
import type {MultiPageScanControllerInterface, MultiPageScanControllerRemote, ScanJobObserverRemote, Scanner, ScannerCapabilities, ScanServiceInterface, ScanSettings as ScanSettingsMojom} from 'chrome://scanning/scanning.mojom-webui.js';
import type {ScanningAppElement} from 'chrome://scanning/scanning_app.js';
import {MAX_NUM_SAVED_SCANNERS} from 'chrome://scanning/scanning_app_types.js';
import type {ScannerCapabilitiesResponse, ScannerSetting, ScannersReceivedResponse, ScanSettings, StartMultiPageScanResponse} from 'chrome://scanning/scanning_app_types.js';
import {getColorModeString, getPageSizeString, tokenToString} from 'chrome://scanning/scanning_app_util.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {changeSelectedIndex, changeSelectedValue, createScanner, createScannerSource} from './scanning_app_test_utils.js';
import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

const MY_FILES_PATH = '/home/chronos/user/MyFiles';

// An arbitrary date needed to replace |lastScanDate| in saved scan settings.
const LAST_SCAN_DATE = new Date('1/1/2021');

// Scanner sources names.
const ADF_DUPLEX = 'adf_duplex';
const ADF_SIMPLEX = 'adf_simplex';
const PLATEN = 'platen';

const firstPageSizes: PageSize[] =
    [PageSize.kIsoA4, PageSize.kNaLetter, PageSize.kMax];
const firstColorModes: ColorMode[] =
    [ColorMode.kBlackAndWhite, ColorMode.kColor];
const firstResolutions: number[] = [75, 100, 300];

const secondPageSizes: PageSize[] = [PageSize.kIsoA4, PageSize.kMax];
const secondColorModes: ColorMode[] =
    [ColorMode.kBlackAndWhite, ColorMode.kGrayscale];
const secondResolutions: number[] = [150, 600];

const thirdPageSizes: PageSize[] = [PageSize.kMax];
const thirdColorModes: ColorMode[] = [ColorMode.kBlackAndWhite];
const thirdResolutions: number[] = [75, 200];

const firstScannerId: UnguessableToken = {
  high: BigInt(0),
  low: BigInt(1),
};
const firstScannerName = 'Scanner 1';

const secondScannerId: UnguessableToken = {
  high: BigInt(0),
  low: BigInt(2),
};
const secondScannerName = 'Scanner 2';

const firstCapabilities = {
  sources: [
    createScannerSource(
        SourceType.kAdfDuplex, ADF_DUPLEX, firstPageSizes, firstColorModes,
        firstResolutions),
    createScannerSource(
        SourceType.kFlatbed, PLATEN, secondPageSizes, firstColorModes,
        firstResolutions),
  ],
};

const secondCapabilities = {
  sources: [
    createScannerSource(
        SourceType.kAdfDuplex, ADF_DUPLEX, thirdPageSizes, thirdColorModes,
        thirdResolutions),
    createScannerSource(
        SourceType.kAdfSimplex, ADF_SIMPLEX, secondPageSizes, secondColorModes,
        secondResolutions),
  ],
};

class FakeScanService implements ScanServiceInterface {
  resolverMap = new Map<string, PromiseResolver<void>>();
  multiPageScanController: MultiPageScanControllerInterface|null = null;
  scanners: Scanner[] = [];
  capabilities = new Map<UnguessableToken, ScannerCapabilities>();
  scanJobObserverRemote: ScanJobObserverRemote|null = null;
  failStartScan = false;


  constructor() {
    this.resetForTest();
  }

  resetForTest() {
    this.scanners = [];
    this.capabilities = new Map();
    this.scanJobObserverRemote = null;
    this.failStartScan = false;
    this.resolverMap.set('getScanners', new PromiseResolver());
    this.resolverMap.set('getScannerCapabilities', new PromiseResolver());
    this.resolverMap.set('startScan', new PromiseResolver());
    this.resolverMap.set('startMultiPageScan', new PromiseResolver());
    this.resolverMap.set('cancelScan', new PromiseResolver());
  }

  private getResolver(methodName: string): PromiseResolver<void> {
    const method = this.resolverMap.get(methodName);
    assertTrue(!!method, `Method '${methodName}' not found.`);
    return method!;
  }

  protected methodCalled(methodName: string) {
    this.getResolver(methodName).resolve();
  }

  whenCalled(methodName: string): Promise<void> {
    return this.getResolver(methodName).promise.then(() => {
      // Support sequential calls to whenCalled() by replacing the promise.
      this.resolverMap.set(methodName, new PromiseResolver());
    });
  }

  setMultiPageScanController(controller: MultiPageScanControllerInterface):
      void {
    this.multiPageScanController = controller;
  }

  setScanners(scanners: Scanner[]): void {
    this.scanners = scanners;
  }

  addScanner(scanner: Scanner): void {
    this.scanners = this.scanners.concat(scanner);
  }

  setCapabilities(capabilities: Map<UnguessableToken, ScannerCapabilities>):
      void {
    this.capabilities = capabilities;
  }

  setFailStartScan(failStartScan: boolean): void {
    this.failStartScan = failStartScan;
  }

  simulateProgress(pageNumber: number, progressPercent: number): Promise<void> {
    assert(this.scanJobObserverRemote);
    this.scanJobObserverRemote.onPageProgress(pageNumber, progressPercent);
    return flushTasks();
  }

  /**
   * @param {number} pageNumber Always 1 for Flatbed scans, increments for ADF
   *    scans.
   * @param {number} newPageIndex The index the new page should take in the
   *    objects array.
   */
  simulatePageComplete(pageNumber: number, newPageIndex: number):
      Promise<void> {
    assert(this.scanJobObserverRemote);
    this.scanJobObserverRemote.onPageProgress(pageNumber, 100);
    const fakePageData = [2, 57, 13, 289];
    this.scanJobObserverRemote.onPageComplete(fakePageData, newPageIndex);
    return flushTasks();
  }

  simulateScanComplete(result: ScanResult, scannedFilePaths: FilePath[]):
      Promise<void> {
    assert(this.scanJobObserverRemote);
    this.scanJobObserverRemote.onScanComplete(result, scannedFilePaths);
    return flushTasks();
  }

  simulateCancelComplete(success: boolean): Promise<void> {
    assert(this.scanJobObserverRemote);
    this.scanJobObserverRemote.onCancelComplete(success);
    return flushTasks();
  }

  simulateMultiPageScanFail(scanResult: ScanResult): Promise<void> {
    assert(this.scanJobObserverRemote);
    this.scanJobObserverRemote.onMultiPageScanFail(scanResult);
    return flushTasks();
  }

  // scanService methods:

  getScanners(): Promise<ScannersReceivedResponse> {
    return new Promise(resolve => {
      this.methodCalled('getScanners');
      resolve({scanners: this.scanners || []});
    });
  }

  getScannerCapabilities(scannerId: UnguessableToken):
      Promise<ScannerCapabilitiesResponse> {
    return new Promise(resolve => {
      this.methodCalled('getScannerCapabilities');
      const capabilities = this.capabilities.get(scannerId);
      assert(capabilities);
      resolve({capabilities});
    });
  }

  startScan(
      scannerId: UnguessableToken, settings: ScanSettingsMojom,
      remote: ScanJobObserverRemote): Promise<{success: boolean}> {
    assert(scannerId);
    assert(settings);
    return new Promise(resolve => {
      this.scanJobObserverRemote = remote;
      this.methodCalled('startScan');
      resolve({success: !this.failStartScan});
    });
  }

  startMultiPageScan(
      scannerId: UnguessableToken, settings: ScanSettingsMojom,
      remote: ScanJobObserverRemote): Promise<StartMultiPageScanResponse> {
    assert(scannerId);
    assert(settings);
    return new Promise(resolve => {
      this.scanJobObserverRemote = remote;
      this.methodCalled('startMultiPageScan');
      const controller = this.failStartScan ?
          null :
          (this.multiPageScanController as MultiPageScanControllerRemote);
      resolve({controller});
    });
  }

  cancelScan() {
    this.methodCalled('cancelScan');
  }
}

class FakeMultiPageScanController implements MultiPageScanControllerInterface {
  resolverMap = new Map<string, PromiseResolver<void>>();
  $ = {
    close() {},
  };
  pageIndexToRemove = -1;
  pageIndexToRescan = -1;


  constructor() {
    this.resetForTest();
  }

  resetForTest(): void {
    this.resolverMap.set('scanNextPage', new PromiseResolver());
    this.resolverMap.set('completeMultiPageScan', new PromiseResolver());
    this.resolverMap.set('rescanPage', new PromiseResolver());
  }

  private getResolver(methodName: string): PromiseResolver<void> {
    const method = this.resolverMap.get(methodName);
    assertTrue(!!method, `Method '${methodName}' not found.`);
    return method!;
  }

  protected methodCalled(methodName: string): void {
    this.getResolver(methodName).resolve();
  }

  whenCalled(methodName: string): Promise<void> {
    return this.getResolver(methodName).promise.then(() => {
      // Support sequential calls to whenCalled() by replacing the promise.
      this.resolverMap.set(methodName, new PromiseResolver());
    });
  }

  scanNextPage(scannerId: UnguessableToken, settings: ScanSettingsMojom):
      Promise<{success: boolean}> {
    assert(scannerId);
    assert(settings);
    return new Promise(resolve => {
      this.methodCalled('scanNextPage');
      resolve({success: true});
    });
  }

  removePage(pageIndex: number): void {
    this.pageIndexToRemove = pageIndex;
  }

  rescanPage(
      scannerId: UnguessableToken, settings: ScanSettingsMojom,
      pageIndex: number): Promise<{success: boolean}> {
    assert(scannerId);
    assert(settings);
    this.pageIndexToRescan = pageIndex;
    return new Promise(resolve => {
      this.methodCalled('rescanPage');
      resolve({success: true});
    });
  }

  completeMultiPageScan(): void {
    this.methodCalled('completeMultiPageScan');
  }

  getPageIndexToRemove(): number {
    return this.pageIndexToRemove;
  }

  getPageIndexToRescan(): number {
    return this.pageIndexToRescan;
  }
}

suite('scanningAppTest', function() {
  let scanningApp: ScanningAppElement|null = null;
  let fakeScanService: FakeScanService;
  let fakeMultiPageScanController: FakeMultiPageScanController;
  let testBrowserProxy: TestScanningBrowserProxy;
  let scannerSelect: HTMLSelectElement|null = null;
  let sourceSelect: HTMLSelectElement|null = null;
  let scanToSelect: HTMLSelectElement|null = null;
  let fileTypeSelect: HTMLSelectElement|null = null;
  let colorModeSelect: HTMLSelectElement|null = null;
  let pageSizeSelect: HTMLSelectElement|null = null;
  let resolutionSelect: HTMLSelectElement|null = null;
  let scanButton: CrButtonElement|null = null;
  let cancelButton: CrButtonElement|null = null;
  let helperText: HTMLElement|null = null;
  let scanProgress: HTMLElement|null = null;
  let progressText: HTMLElement|null = null;
  let progressBar: PaperProgressElement|null = null;
  let scannedImages: HTMLElement|null = null;
  let linkEl: HTMLLinkElement|null = null;

  const capabilities = new Map<UnguessableToken, ScannerCapabilities>();
  capabilities.set(firstScannerId, firstCapabilities);
  capabilities.set(secondScannerId, secondCapabilities);
  const expectedScanners: Scanner[] = [
    createScanner(firstScannerId, firstScannerName),
    createScanner(secondScannerId, secondScannerName),
  ];

  suiteSetup(() => {
    fakeScanService = new FakeScanService();
    setScanServiceForTesting(fakeScanService);
    fakeMultiPageScanController = new FakeMultiPageScanController();
    testBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.setInstance(testBrowserProxy);
    testBrowserProxy.setMyFilesPath(MY_FILES_PATH);
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    linkEl = document.createElement('link');
    document.head.appendChild(linkEl);
  });

  teardown(function() {
    fakeScanService.resetForTest();
    fakeMultiPageScanController.resetForTest();
    scanningApp?.remove();
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

  function initializeScanningApp(
      scanners: Scanner[],
      capabilities: Map<UnguessableToken, ScannerCapabilities>): Promise<void> {
    fakeScanService.setMultiPageScanController(fakeMultiPageScanController);
    fakeScanService.setScanners(scanners);
    fakeScanService.setCapabilities(capabilities);
    scanningApp = document.createElement('scanning-app');
    document.body.appendChild(scanningApp);
    assertTrue(!!scanningApp);
    assertTrue(isVisible(
        strictQuery('loading-page', scanningApp.shadowRoot, HTMLElement)));
    return fakeScanService.whenCalled('getScanners');
  }

  /**
   * Returns the "More settings" button.
   */
  function getMoreSettingsButton(): CrButtonElement {
    assert(scanningApp);
    const button = strictQuery(
        '#moreSettingsButton', scanningApp.shadowRoot, CrButtonElement);
    assertTrue(!!button);
    return button;
  }

  /**
   * Clicks the "More settings" button.
   */
  function clickMoreSettingsButton(): Promise<void> {
    getMoreSettingsButton().click();
    return flushTasks();
  }

  /**
   * Clicks the "Done" button.
   */
  function clickDoneButton(): Promise<void> {
    assert(scanningApp);
    const doneSection =
        strictQuery('scan-done-section', scanningApp.shadowRoot, HTMLElement);
    assert(doneSection);
    const button =
        strictQuery('#doneButton', doneSection.shadowRoot, CrButtonElement);
    assert(button);
    button.click();
    return flushTasks();
  }

  /**
   * Clicks the "Ok" button to close the scan failed dialog.
   */
  function clickScanFailedDialogOkButton(): Promise<void> {
    assert(scanningApp);
    const button =
        strictQuery('#okButton', scanningApp.shadowRoot, CrButtonElement);
    assertTrue(!!button);
    button.click();
    return flushTasks();
  }

  /**
   * Returns whether the "More settings" section is expanded or not.
   */
  function isSettingsOpen(): boolean {
    assert(scanningApp);
    const collapse: IronCollapseElement =
        scanningApp.shadowRoot!.querySelector('#collapse')!;
    assert(collapse);
    return collapse.opened;
  }

  /**
   * Fetches capabilities then waits for app to change to READY state.
   */
  async function getScannerCapabilities(): Promise<void> {
    // On AppState === READY, the last step is to set the scanner select element
    // as focused. Use focus event to ensure setup steps have completed before
    // continuing with test.
    const onReadyFocus =
        eventToPromise('focus', getSettingSelect('#scannerSelect'));
    await fakeScanService.whenCalled('getScannerCapabilities');
    return onReadyFocus;
  }

  /**
   * Deep equals two ScanSettings objects.
   */
  function compareSavedScanSettings(
      expectedScanSettings: ScanSettings,
      actualScanSettings: ScanSettings): void {
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

  function getSettingSelect(parentSelector: string): HTMLSelectElement {
    assert(scanningApp);
    const parentEl =
        strictQuery(parentSelector, scanningApp.shadowRoot, HTMLElement);
    assert(parentEl);
    return strictQuery('select', parentEl.shadowRoot, HTMLSelectElement)!;
  }

  function ensureMultiPageScanCheckboxChecked(checked: boolean): Promise<void> {
    assert(scanningApp);
    const multiPageScanCheckbox =
        strictQuery('multi-page-checkbox', scanningApp.shadowRoot, HTMLElement);
    assertTrue(isVisible(multiPageScanCheckbox));
    const checkbox = strictQuery(
        'cr-checkbox', multiPageScanCheckbox.shadowRoot, CrCheckboxElement);
    assertTrue(isVisible(checkbox));

    if (checkbox.checked !== checked) {
      checkbox.click();
    }

    return flushTasks();
  }

  // Verify a full scan job can be completed.
  test('Scan', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    assertFalse(isVisible(
        strictQuery('loading-page', scanningApp.shadowRoot, HTMLElement)));

    scannerSelect = getSettingSelect('#scannerSelect');
    sourceSelect = getSettingSelect('#sourceSelect');
    scanToSelect = getSettingSelect('#scanToSelect');
    fileTypeSelect = getSettingSelect('#fileTypeSelect');
    colorModeSelect = getSettingSelect('#colorModeSelect');
    pageSizeSelect = getSettingSelect('#pageSizeSelect');
    resolutionSelect = getSettingSelect('#resolutionSelect');
    scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement);
    cancelButton =
        strictQuery('#cancelButton', scanningApp.shadowRoot, CrButtonElement);
    const scanPreview =
        strictQuery('#scanPreview', scanningApp.shadowRoot, ScanPreviewElement);
    helperText =
        strictQuery('#helperText', scanPreview.shadowRoot, HTMLElement);
    scanProgress =
        strictQuery('#scanProgress', scanPreview.shadowRoot, HTMLElement);
    progressText =
        strictQuery('#progressText', scanPreview.shadowRoot, HTMLElement);
    progressBar = scanPreview.shadowRoot!.querySelector('paper-progress');
    scannedImages =
        strictQuery('#scannedImages', scanPreview.shadowRoot, HTMLElement);
    await getScannerCapabilities();

    assertEquals(tokenToString(firstScannerId), scanningApp.selectedScannerId);
    // A scanner with type "FLATBED" will be used as the selectedSource
    // if it exists.
    assertEquals(
        firstCapabilities.sources[1]!.name, scanningApp.selectedSource);
    assertEquals(MY_FILES_PATH, scanningApp.selectedFilePath);
    assertEquals(FileType.kPdf.toString(), scanningApp.selectedFileType);
    assertEquals(ColorMode.kColor.toString(), scanningApp.selectedColorMode);
    assertEquals(
        firstCapabilities.sources[1]!.pageSizes[0]!.toString(),
        scanningApp.selectedPageSize);
    assertEquals(
        firstCapabilities.sources[1]!.resolutions[0]!.toString(),
        scanningApp.selectedResolution);

    // Before the scan button is clicked, the settings and scan button
    // should be enabled, and the helper text should be displayed.
    assertFalse(scannerSelect!.disabled);
    assertFalse(sourceSelect!.disabled);
    assertFalse(scanToSelect!.disabled);
    assertFalse(fileTypeSelect!.disabled);
    assertFalse(colorModeSelect!.disabled);
    assertFalse(pageSizeSelect!.disabled);
    assertFalse(resolutionSelect!.disabled);
    assertFalse(scanButton!.disabled);
    assertTrue(isVisible(scanButton));
    assertEquals('Scan', scanButton!.textContent!.trim());
    assertFalse(isVisible(cancelButton));
    assertTrue(isVisible(helperText));
    assertFalse(isVisible(scanProgress));
    assertFalse(isVisible(progressBar));
    assertFalse(
        isVisible(scanningApp.shadowRoot!.querySelector('scan-done-section')));

    // Click the Scan button and wait till the scan is started.
    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    // After the scan button is clicked and the scan has started, the
    // settings and scan button should be disabled, and the progress bar
    // and text should be visible and indicate that scanning is in
    // progress.
    assertTrue(scannerSelect!.disabled);
    assertTrue(sourceSelect!.disabled);
    assertTrue(scanToSelect!.disabled);
    assertTrue(fileTypeSelect!.disabled);
    assertTrue(colorModeSelect!.disabled);
    assertTrue(pageSizeSelect!.disabled);
    assertTrue(resolutionSelect!.disabled);
    assertTrue(scanButton!.disabled);
    assertFalse(isVisible(scanButton));
    assertTrue(isVisible(cancelButton));
    assertFalse(isVisible(helperText));
    assertTrue(isVisible(scanProgress));
    assertFalse(
        isVisible(scanningApp.shadowRoot!.querySelector('scan-done-section')));
    assertEquals('Scanning page 1', progressText!.textContent!.trim());
    assertEquals(0, progressBar!.value);

    // Simulate a progress update and verify the progress bar and text are
    // updated correctly.
    await fakeScanService.simulateProgress(1, 17);

    assertEquals('Scanning page 1', progressText!.textContent!.trim());
    assertEquals(17, progressBar!.value);

    // Simulate a page complete update and verify the progress bar and
    // text are updated correctly.
    let newPageIndex = 0;
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, ++newPageIndex);

    assertEquals('Scanning page 1', progressText!.textContent!.trim());
    assertEquals(100, progressBar!.value);

    // Simulate a progress update for a second page and verify the
    // progress bar and text are updated correctly.
    await fakeScanService.simulateProgress(2, 53);

    assertEquals('Scanning page 2', progressText!.textContent!.trim());
    assertEquals(53, progressBar!.value);

    // Complete the page.
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 2, ++newPageIndex);

    // Complete the scan.
    const scannedFilePaths: FilePath[] =
        [{'path': '/test/path/scan1.jpg'}, {'path': '/test/path/scan2.jpg'}];
    await fakeScanService.simulateScanComplete(
        ScanResult.kSuccess, scannedFilePaths);

    assertTrue(isVisible(scannedImages));
    assertEquals(2, scannedImages!.querySelectorAll('img').length);
    const doneSection =
        scanningApp.shadowRoot!.querySelector('scan-done-section');
    assertTrue(isVisible(doneSection));
    assertArrayEquals(scannedFilePaths, doneSection!.scannedFilePaths);

    // Click the Done button to await to READY state.
    await clickDoneButton();

    // After scanning is complete, the settings and scan button should be
    // enabled. The progress bar and text should no longer be visible.
    assertFalse(scannerSelect!.disabled);
    assertFalse(sourceSelect!.disabled);
    assertFalse(scanToSelect!.disabled);
    assertFalse(fileTypeSelect!.disabled);
    assertFalse(colorModeSelect!.disabled);
    assertFalse(pageSizeSelect!.disabled);
    assertFalse(resolutionSelect!.disabled);
    assertFalse(scanButton!.disabled);
    assertTrue(isVisible(scanButton));
    assertFalse(isVisible(cancelButton));
    assertTrue(isVisible(helperText));
    assertFalse(isVisible(scanProgress));
    assertFalse(isVisible(
        strictQuery('scan-done-section', scanningApp.shadowRoot, HTMLElement)));
    assertFalse(isVisible(scannedImages));
    assertEquals(0, scannedImages!.querySelectorAll('img').length);
  });

  // Verify the scan failed dialog shows when a scan job fails.
  test('ScanFailed', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement);
    await getScannerCapabilities();

    // Click the Scan button and wait till the scan is started.
    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    // Simulate a progress update.
    await fakeScanService.simulateProgress(1, 17);

    // Simulate the scan failing.
    await fakeScanService.simulateScanComplete(ScanResult.kIoError, []);

    // The scan failed dialog should open.
    assertTrue(strictQuery(
                   '#scanFailedDialog', scanningApp.shadowRoot, CrDialogElement)
                   .open);
    // Click the dialog's Ok button to await to READY state.
    await clickScanFailedDialogOkButton();

    // After the dialog closes, the scan button should be enabled and
    // ready to start a new scan.
    assertFalse(
        strictQuery(
            '#scanFailedDialog', scanningApp.shadowRoot, CrDialogElement)
            .open);
    assertFalse(scanButton!.disabled);
    assertTrue(isVisible(scanButton));
  });

  // Verify the scan failed dialog closes and resets the scan app state when the
  // user clicks ESC.
  test('EscClosesScanFailedDialog', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement);
    await getScannerCapabilities();

    // Click the Scan button and wait till the scan is started.
    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    // Simulate a progress update.
    await fakeScanService.simulateProgress(
        /*pageNumber=*/ 1, /*progressPercent=*/ 17);

    // Simulate the scan failing.
    await fakeScanService.simulateScanComplete(ScanResult.kIoError, []);

    const scanFailedDialog = strictQuery(
        '#scanFailedDialog', scanningApp.shadowRoot, CrDialogElement);

    // The scan failed dialog should open.
    assertTrue(scanFailedDialog.open);

    // Simulate the ESC key by sending the `cancel` event to the native
    // dialog.
    strictQuery('#dialog', scanFailedDialog.shadowRoot, HTMLElement)
        .dispatchEvent(new Event('cancel'));
    assertFalse(
        strictQuery(
            '#scanFailedDialog', scanningApp.shadowRoot, CrDialogElement)
            .open);
    assertFalse(scanButton!.disabled);
    assertTrue(isVisible(scanButton));
  });

  // Verify a multi-page scan job can be initiated.
  test('MultiPageScan', async () => {
    const scannedFilePaths = [{'path': '/test/path/scan1.pdf'}];
    let newPageIndex = 0;

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await waitAfterNextRender(scanningApp);

    scanningApp.multiPageScanChecked = true;

    await flushTasks();

    const scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement);
    assertEquals('Scan page 1', scanButton!.textContent!.trim());
    scanButton!.click();
    await fakeScanService.whenCalled('startMultiPageScan');

    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    // The scanned images and multi-page scan page should be visible.
    const scanPreview =
        strictQuery('#scanPreview', scanningApp.shadowRoot, ScanPreviewElement);
    assertTrue(isVisible(
        strictQuery('#scannedImages', scanPreview.shadowRoot, HTMLElement)));
    const multiPageScan =
        strictQuery('multi-page-scan', scanningApp.shadowRoot, HTMLElement);
    assertTrue(isVisible(multiPageScan));

    const scanNextPageButton =
        strictQuery('#scanButton', multiPageScan.shadowRoot, CrButtonElement);
    assertEquals('Scan page 2', scanNextPageButton.textContent!.trim());
    scanNextPageButton.click();
    await fakeMultiPageScanController.whenCalled('scanNextPage');

    // Cancel button should be visible while scanning.
    assertFalse(isVisible(scanNextPageButton));
    const cancelButton =
        strictQuery('#cancelButton', multiPageScan.shadowRoot, CrButtonElement);
    assertTrue(isVisible(cancelButton));

    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    // The scanned images and multi-page scan page should still be visible
    // after scanning the next page.
    assertTrue(isVisible(
        strictQuery('#scannedImages', scanPreview.shadowRoot, HTMLElement)));
    assertTrue(isVisible(multiPageScan));

    assertEquals('Scan page 3', scanNextPageButton.textContent!.trim());

    strictQuery('#saveButton', multiPageScan.shadowRoot, CrButtonElement)
        .click();
    await fakeMultiPageScanController.whenCalled('completeMultiPageScan');

    await fakeScanService.simulateScanComplete(
        ScanResult.kSuccess, scannedFilePaths);

    scannedImages =
        strictQuery('#scannedImages', scanPreview.shadowRoot, HTMLElement);
    assertTrue(isVisible(scannedImages));
    const doneSection = strictQuery(
        'scan-done-section', scanningApp.shadowRoot, ScanDoneSectionElement);
    assertTrue(isVisible(doneSection));
    assertArrayEquals(scannedFilePaths, doneSection.scannedFilePaths);
  });

  // Verify a multi-page scan job can fail scanning a page then scan another
  // page successfully.
  test('MultiPageScanPageFailed', async () => {
    let newPageIndex = 0;

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await waitAfterNextRender(scanningApp);

    scanningApp.multiPageScanChecked = true;

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await fakeScanService.whenCalled('startMultiPageScan');

    const scanPreview =
        strictQuery('#scanPreview', scanningApp.shadowRoot, HTMLElement);
    const progressText =
        strictQuery('#progressText', scanPreview.shadowRoot, HTMLElement);
    assertEquals('Scanning page 1', progressText.textContent!.trim());
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    const multiPageScan =
        strictQuery('multi-page-scan', scanningApp.shadowRoot, HTMLElement);
    const scanButton =
        strictQuery('#scanButton', multiPageScan.shadowRoot, CrButtonElement);
    scanButton.click();
    await fakeMultiPageScanController.whenCalled('scanNextPage');

    assertEquals('Scanning page 2', progressText.textContent!.trim());
    await fakeScanService.simulateMultiPageScanFail(ScanResult.kFlatbedOpen);

    // The scan failed dialog should open.
    assertTrue(strictQuery(
                   '#scanFailedDialog', scanningApp.shadowRoot, CrDialogElement)
                   .open);
    assertEquals(
        loadTimeData.getString('scanFailedDialogFlatbedOpenText'),
        strictQuery(
            '#scanFailedDialogText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());

    // Click the dialog's Ok button to await to MULTI_PAGE_NEXT_ACTION
    // state.
    await clickScanFailedDialogOkButton();

    // After the dialog closes, the scan next page button should still
    // say 'Scan Page 2'.
    assertEquals('Scan page 2', scanButton.textContent!.trim());
    scanButton.click();
    await fakeMultiPageScanController.whenCalled('scanNextPage');

    assertEquals('Scanning page 2', progressText.textContent!.trim());
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    strictQuery('#saveButton', multiPageScan.shadowRoot, CrButtonElement)
        .click();
    await fakeMultiPageScanController.whenCalled('completeMultiPageScan');

    await fakeScanService.simulateScanComplete(
        ScanResult.kSuccess, [{'path': '/test/path/scan1.pdf'}]);

    scannedImages =
        strictQuery('#scannedImages', scanPreview.shadowRoot, HTMLElement);

    // There should be 2 images from scanning once, failing once, then
    // scanning again successfully.
    assertEquals(2, scannedImages!.querySelectorAll('img').length);
  });

  // Verify a scan can be canceled during a multi-page scan session.
  test('MultiPageCancelScan', async () => {
    let newPageIndex = 0;

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await flushTasks();

    scanningApp.multiPageScanChecked = true;
    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await fakeScanService.whenCalled('startMultiPageScan');

    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    const multiPageScan =
        strictQuery('multi-page-scan', scanningApp.shadowRoot, HTMLElement);
    const scanButton =
        strictQuery('#scanButton', multiPageScan.shadowRoot, CrButtonElement);
    scanButton.click();
    await fakeMultiPageScanController.whenCalled('scanNextPage');

    // Click the Cancel button to cancel the scan.
    const cancelButton =
        strictQuery('#cancelButton', multiPageScan.shadowRoot, CrButtonElement);
    cancelButton.click();
    await fakeScanService.whenCalled('cancelScan');

    // Cancel button should be disabled while canceling is in progress.
    assertTrue(cancelButton.disabled);

    // Simulate cancel completing successfully.
    await fakeScanService.simulateCancelComplete(true);

    // After canceling is complete, the Scan Next Page button should be
    // visible and showing the correct page number to scan. The cancel
    // button should be hidden.
    assertTrue(isVisible(scanButton));
    assertEquals('Scan page 2', scanButton.textContent!.trim());
    assertFalse(isVisible(cancelButton));
    assertTrue(
        strictQuery('#toast', scanningApp.shadowRoot, CrToastElement).open);
  });

  // Verify the correct page can be removed from a multi-page scan job by
  // scanning three pages then removing the second page.
  test('MultiPageScanPageRemoved', async () => {
    const pageIndexToRemove = 1;
    let newPageIndex = 0;

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await waitAfterNextRender(scanningApp);

    scanningApp.multiPageScanChecked = true;

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await fakeScanService.whenCalled('startMultiPageScan');

    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    const multiPageScan =
        strictQuery('multi-page-scan', scanningApp.shadowRoot, HTMLElement);
    const scanButton =
        strictQuery('#scanButton', multiPageScan.shadowRoot, CrButtonElement);
    scanButton.click();
    await fakeMultiPageScanController.whenCalled('scanNextPage');

    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    scanButton.click();
    await fakeMultiPageScanController.whenCalled('scanNextPage');

    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    // Save the current scanned images
    const scanPreview =
        strictQuery('#scanPreview', scanningApp.shadowRoot, ScanPreviewElement);
    const expectedObjectUrls = scanPreview.objectUrls;
    assertEquals(3, expectedObjectUrls.length);

    // Open the remove page dialog.
    scanPreview.shadowRoot!.querySelector('action-toolbar')!.dispatchEvent(
        new CustomEvent(
            'show-remove-page-dialog', {detail: pageIndexToRemove}));
    await flushTasks();

    const actionButton =
        strictQuery('#actionButton', scanPreview.shadowRoot, CrButtonElement);
    actionButton.click();
    await flushTasks();

    assertEquals(
        pageIndexToRemove, fakeMultiPageScanController.getPageIndexToRemove());

    // Remove the second page from the expected scanned images and verify
    // the correct image was removed from the actual scanned images.
    expectedObjectUrls.splice(pageIndexToRemove, 1);
    assertArrayEquals(expectedObjectUrls, scanPreview.objectUrls);
  });

  // Verify if there's only one page in the multi-page scan session it can be
  // removed, the scan is reset, and the user is awaited to the scan settings
  // page.
  test('MultiPageScanRemoveLastPage', async () => {
    let newPageIndex = 0;

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await waitAfterNextRender(scanningApp);

    scanningApp.multiPageScanChecked = true;

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await fakeScanService.whenCalled('startMultiPageScan');

    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    // Open the remove page dialog.
    const scanPreview =
        strictQuery('#scanPreview', scanningApp.shadowRoot, ScanPreviewElement);
    scanPreview.shadowRoot!.querySelector('action-toolbar')!.dispatchEvent(
        new CustomEvent('show-remove-page-dialog', {detail: 0}));
    await flushTasks();

    strictQuery('#actionButton', scanPreview.shadowRoot, CrButtonElement)
        .click();
    await flushTasks();

    assertArrayEquals([], scanPreview.objectUrls);
    --newPageIndex;

    // Attempt a new multi-page scan from the scan settings page.
    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await fakeScanService.whenCalled('startMultiPageScan');

    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    // The scanned images and multi-page scan page should be visible.
    assertTrue(
        isVisible(scanPreview.shadowRoot!.querySelector('#scannedImages')));
    assertTrue(isVisible(
        strictQuery('multi-page-scan', scanningApp.shadowRoot, HTMLElement)));

    const scanNextPageButton =
        strictQuery('multi-page-scan', scanningApp.shadowRoot, HTMLElement)
            .shadowRoot!.querySelector('#scanButton');
    assert(scanNextPageButton);
    assertEquals('Scan page 2', scanNextPageButton.textContent!.trim());
  });

  // Verify one page can be scanned and then rescanned in a multi-page scan job.
  test('MultiPageScanRescanOnePage', async () => {
    const scannedFilePaths: FilePath[] = [{'path': '/test/path/scan1.pdf'}];
    const pageIndexToRescan = 0;

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    const scanPreview =
        strictQuery('#scanPreview', scanningApp.shadowRoot, ScanPreviewElement);
    await getScannerCapabilities();

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await waitAfterNextRender(scanningApp);

    scanningApp.multiPageScanChecked = true;

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await fakeScanService.whenCalled('startMultiPageScan');

    let newPageIndex = 0;
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, ++newPageIndex);

    // Save the current scanned image.
    const expectedObjectUrls = [...scanPreview.objectUrls];
    assertEquals(1, expectedObjectUrls.length);

    // Open the rescan page dialog.
    const actionToolbar =
        strictQuery('action-toolbar', scanPreview.shadowRoot, HTMLElement);
    actionToolbar.dispatchEvent(new CustomEvent(
        'show-rescan-page-dialog', {detail: pageIndexToRescan}));
    await flushTasks();

    // Verify the dialog shows we are rescanning the correct page number.
    assertEquals(
        'Rescan page?',
        strictQuery('#dialogTitle', scanPreview.shadowRoot, HTMLElement)
            .textContent!.trim());

    strictQuery('#actionButton', scanPreview.shadowRoot, HTMLElement).click();
    await fakeMultiPageScanController.whenCalled('rescanPage');

    // Verify the progress text shows we are attempting to rescan the
    // first page.
    progressText =
        strictQuery('#progressText', scanPreview.shadowRoot, HTMLElement);
    assertEquals('Scanning page 1', progressText!.textContent!.trim());
    assertEquals(
        pageIndexToRescan, fakeMultiPageScanController.getPageIndexToRescan());
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, /*newPageIndex=*/ 0);

    // After rescanning verify the page is different.
    const actualObjectUrls = scanPreview.objectUrls;
    assertEquals(1, actualObjectUrls.length);
    assertNotEquals(expectedObjectUrls[0]!, actualObjectUrls[0]!);

    // Save the one page scan.
    const multiPageScan =
        strictQuery('multi-page-scan', scanningApp.shadowRoot, HTMLElement);
    const saveButton =
        strictQuery('#saveButton', multiPageScan.shadowRoot, CrButtonElement);
    assert(saveButton);
    saveButton.click();
    await fakeMultiPageScanController.whenCalled('completeMultiPageScan');
    await fakeScanService.simulateScanComplete(
        ScanResult.kSuccess, scannedFilePaths);

    scannedImages =
        strictQuery('#scannedImages', scanPreview.shadowRoot, HTMLElement);
    assertTrue(isVisible(scannedImages));
    const doneSection = strictQuery(
        'scan-done-section', scanningApp.shadowRoot, ScanDoneSectionElement);
    assertTrue(isVisible(doneSection));
    assertArrayEquals(scannedFilePaths, doneSection.scannedFilePaths);
  });

  // Verify a page can be rescanned in a multi-page scan job. This test
  // simulates scanning two pages, rescanning the first page, then scanning a
  // third page.
  test('MultiPageScanPageRescanned', async () => {
    const pageIndexToRescan = 0;

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    const scanPreview =
        strictQuery('#scanPreview', scanningApp.shadowRoot, ScanPreviewElement);
    await getScannerCapabilities();

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await waitAfterNextRender(scanningApp);

    scanningApp.multiPageScanChecked = true;

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await fakeScanService.whenCalled('startMultiPageScan');

    let newPageIndex = 0;
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    const multiPageScan =
        strictQuery('multi-page-scan', scanningApp.shadowRoot, HTMLElement);
    const scanButton =
        strictQuery('#scanButton', multiPageScan.shadowRoot, CrButtonElement);
    scanButton.click();
    await fakeMultiPageScanController.whenCalled('scanNextPage');

    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    // Save the current scanned images.
    const expectedObjectUrls = [...scanPreview.objectUrls];
    assertEquals(2, expectedObjectUrls.length);

    // Open the rescan page dialog.
    strictQuery('action-toolbar', scanPreview.shadowRoot, HTMLElement)
        .dispatchEvent(new CustomEvent(
            'show-rescan-page-dialog', {detail: pageIndexToRescan}));
    await flushTasks();

    // Verify the dialog shows we are rescanning the correct page number.
    assertEquals(
        'Rescan page 1?',
        strictQuery('#dialogTitle', scanPreview.shadowRoot, HTMLElement)
            .textContent!.trim());

    strictQuery('#actionButton', scanPreview.shadowRoot, HTMLElement).click();
    await fakeMultiPageScanController.whenCalled('rescanPage');

    // Verify the progress text shows we are attempting to rescan the
    // first page.
    progressText =
        strictQuery('#progressText', scanPreview.shadowRoot, HTMLElement);
    assertEquals('Scanning page 1', progressText!.textContent!.trim());
    assertEquals(
        pageIndexToRescan, fakeMultiPageScanController.getPageIndexToRescan());
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, /*newPageIndex=*/ 0);

    // After rescanning verify that the first page changed but the second
    // page stayed the same.
    const actualObjectUrls = scanPreview.objectUrls;
    assertEquals(2, actualObjectUrls.length);
    assertNotEquals(expectedObjectUrls[0]!, actualObjectUrls[0]!);
    assertEquals(expectedObjectUrls[1]!, actualObjectUrls[1]!);

    // Verify that after rescanning, the scan button shows the correct
    // next page number to scan.
    assertEquals('Scan page 3', scanButton!.textContent!.trim());

    scanButton!.click();
    await fakeMultiPageScanController.whenCalled('scanNextPage');

    // Verify the progress text shows we are scanning the third page.
    assertEquals('Scanning page 3', progressText!.textContent!.trim());
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    assertEquals(3, scanPreview.objectUrls.length);
  });

  // Verify that if rescanning a page fails, the page numbers update correctly.
  test('MultiPageScanPageRescanFail', async () => {
    const pageIndexToRescan = 0;

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    const scanPreview =
        strictQuery('#scanPreview', scanningApp.shadowRoot, ScanPreviewElement);
    await getScannerCapabilities();

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await waitAfterNextRender(scanningApp);

    scanningApp.multiPageScanChecked = true;

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await fakeScanService.whenCalled('startMultiPageScan');

    let newPageIndex = 0;
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    const multiPageScan =
        strictQuery('multi-page-scan', scanningApp.shadowRoot, HTMLElement);
    const scanButton =
        strictQuery('#scanButton', multiPageScan.shadowRoot, CrButtonElement);
    scanButton.click();
    await fakeMultiPageScanController.whenCalled('scanNextPage');
    await fakeScanService.simulatePageComplete(
        /*pageNumber=*/ 1, newPageIndex++);

    // Save the current scanned images.
    const expectedObjectUrls = [...scanPreview.objectUrls];
    assertEquals(2, expectedObjectUrls.length);

    // Open the rescan page dialog.
    strictQuery('action-toolbar', scanPreview.shadowRoot, HTMLElement)
        .dispatchEvent(new CustomEvent(
            'show-rescan-page-dialog', {detail: pageIndexToRescan}));
    await flushTasks();

    strictQuery('#actionButton', scanPreview.shadowRoot, HTMLElement).click();
    await fakeMultiPageScanController.whenCalled('rescanPage');

    await fakeScanService.simulateMultiPageScanFail(ScanResult.kFlatbedOpen);

    // The scan failed dialog should open.
    assertTrue(strictQuery(
                   '#scanFailedDialog', scanningApp.shadowRoot, CrDialogElement)
                   .open);
    assertEquals(
        loadTimeData.getString('scanFailedDialogFlatbedOpenText'),
        strictQuery(
            '#scanFailedDialogText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());

    // Click the dialog's Ok button to await to MULTI_PAGE_NEXT_ACTION
    // state.
    await clickScanFailedDialogOkButton();

    // Verify that the pages stayed the same.
    const actualObjectUrls = scanPreview.objectUrls;
    assertEquals(2, actualObjectUrls.length);
    assertArrayEquals(expectedObjectUrls, actualObjectUrls);

    // Verify the scan button shows the correct next page number to scan.
    assertEquals('Scan page 3', scanButton.textContent!.trim());
  });

  // Verify the page size, color, and resolution dropdowns contain the correct
  // elements when each source is selected.
  test('SourceChangeUpdatesDropdowns', async () => {
    await initializeScanningApp(expectedScanners.slice(1), capabilities);
    assert(scanningApp);
    sourceSelect = getSettingSelect('#sourceSelect');
    await getScannerCapabilities();

    assertEquals(2, sourceSelect!.length);
    await changeSelectedIndex(sourceSelect!, /*index=*/ 0);

    colorModeSelect = getSettingSelect('#colorModeSelect');
    pageSizeSelect = getSettingSelect('#pageSizeSelect');
    resolutionSelect = getSettingSelect('#resolutionSelect');

    assertEquals(2, colorModeSelect!.length);
    assertEquals(
        getColorModeString(secondColorModes[0]!),
        colorModeSelect!.options[0]!.textContent!.trim());
    assertEquals(
        getColorModeString(secondColorModes[1]!),
        colorModeSelect!.options[1]!.textContent!.trim());
    assertEquals(2, pageSizeSelect!.length);
    assertEquals(
        getPageSizeString(secondPageSizes[0]!),
        pageSizeSelect!.options[0]!.textContent!.trim());
    assertEquals(
        getPageSizeString(secondPageSizes[1]!),
        pageSizeSelect!.options[1]!.textContent!.trim());
    assertEquals(2, resolutionSelect!.length);
    assertEquals(
        secondResolutions[0]!.toString() + ' dpi',
        resolutionSelect!.options[0]!.textContent!.trim());
    assertEquals(
        secondResolutions[1]!.toString() + ' dpi',
        resolutionSelect!.options[1]!.textContent!.trim());
    await changeSelectedIndex(sourceSelect!, /*index=*/ 1);

    assertEquals(1, colorModeSelect!.length);
    assertEquals(
        getColorModeString(thirdColorModes[0]!),
        colorModeSelect!.options[0]!!.textContent!.trim());
    assertEquals(1, pageSizeSelect!.length);
    assertEquals(
        getPageSizeString(thirdPageSizes[0]!),
        pageSizeSelect!.options[0]!.textContent!.trim());
    assertEquals(2, resolutionSelect!.length);
    assertEquals(
        thirdResolutions[0]!.toString() + ' dpi',
        resolutionSelect!.options[0]!.textContent!.trim());
    assertEquals(
        thirdResolutions[1]!.toString() + ' dpi',
        resolutionSelect!.options[1]!.textContent!.trim());
  });

  // Verify the correct message is shown in the scan failed dialog based on the
  // error type.
  test('ScanResults', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement);
    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    await fakeScanService.simulateScanComplete(ScanResult.kUnknownError, []);

    assertEquals(
        loadTimeData.getString('scanFailedDialogUnknownErrorText'),
        strictQuery(
            '#scanFailedDialogText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());
    await clickScanFailedDialogOkButton();

    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    await fakeScanService.simulateScanComplete(ScanResult.kDeviceBusy, []);

    assertEquals(
        loadTimeData.getString('scanFailedDialogDeviceBusyText'),
        strictQuery(
            '#scanFailedDialogText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());
    await clickScanFailedDialogOkButton();

    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    await fakeScanService.simulateScanComplete(ScanResult.kAdfJammed, []);

    assertEquals(
        loadTimeData.getString('scanFailedDialogAdfJammedText'),
        strictQuery(
            '#scanFailedDialogText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());
    await clickScanFailedDialogOkButton();

    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    await fakeScanService.simulateScanComplete(ScanResult.kAdfEmpty, []);

    assertEquals(
        loadTimeData.getString('scanFailedDialogAdfEmptyText'),
        strictQuery(
            '#scanFailedDialogText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());
    await clickScanFailedDialogOkButton();

    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    await fakeScanService.simulateScanComplete(ScanResult.kFlatbedOpen, []);

    assertEquals(
        loadTimeData.getString('scanFailedDialogFlatbedOpenText'),
        strictQuery(
            '#scanFailedDialogText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());
    await clickScanFailedDialogOkButton();

    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    await fakeScanService.simulateScanComplete(ScanResult.kIoError, []);

    assertEquals(
        loadTimeData.getString('scanFailedDialogIoErrorText'),
        strictQuery(
            '#scanFailedDialogText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());
    await clickScanFailedDialogOkButton();
  });

  // Verify a scan job can be canceled.
  test('CancelScan', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    const scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement);
    const cancelButton =
        strictQuery('#cancelButton', scanningApp.shadowRoot, CrButtonElement);
    await getScannerCapabilities();

    // Before the scan button is clicked, the scan button should be
    // visible and enabled, and the cancel button shouldn't be visible.
    assertFalse(scanButton!.disabled);
    assertTrue(isVisible(scanButton));
    assertFalse(isVisible(cancelButton));

    // Click the Scan button and wait till the scan is started.
    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    // After the scan button is clicked and the scan has started, the scan
    // button should be disabled and not visible, and the cancel button
    // should be visible.
    assertTrue(scanButton!.disabled);
    assertFalse(isVisible(scanButton));
    assertTrue(isVisible(cancelButton));

    // Simulate a progress update and verify the progress bar and text are
    // updated correctly.
    await fakeScanService.simulateProgress(1, 17);

    // Click the cancel button to cancel the scan.
    cancelButton!.click();
    await fakeScanService.whenCalled('cancelScan');

    // Cancel button should be disabled while canceling is in progress.
    assertTrue(cancelButton!.disabled);
    // Simulate cancel completing successfully.
    await fakeScanService.simulateCancelComplete(true);

    // After canceling is complete, the scan button should be visible and
    // enabled, and the cancel button shouldn't be visible.
    assertTrue(isVisible(scanButton));
    assertFalse(isVisible(cancelButton));
    assertTrue(
        strictQuery('#toast', scanningApp.shadowRoot, CrToastElement).open);
    assertFalse(isVisible(
        strictQuery('#toastInfoIcon', scanningApp.shadowRoot, HTMLElement)));
    assertFalse(isVisible(
        strictQuery('#getHelpLink', scanningApp.shadowRoot, HTMLElement)));
    assertEquals(
        scanningApp.i18n('scanCanceledToastText'),
        strictQuery('#toastText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());
  });

  // Verify the cancel scan failed dialog shows when a scan job fails to cancel.
  test('CancelScanFailed', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement);
    cancelButton =
        strictQuery('#cancelButton', scanningApp.shadowRoot, CrButtonElement);
    await getScannerCapabilities();

    // Click the Scan button and wait till the scan is started.
    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    // Simulate a progress update and verify the progress bar and text are
    // updated correctly.
    await fakeScanService.simulateProgress(1, 17);

    // Click the cancel button to cancel the scan.
    cancelButton!.click();
    assertFalse(
        strictQuery('#toast', scanningApp.shadowRoot, CrToastElement).open);
    await fakeScanService.whenCalled('cancelScan');

    // Cancel button should be disabled while canceling is in progress.
    assertTrue(cancelButton!.disabled);
    // Simulate cancel failing.
    await fakeScanService.simulateCancelComplete(false);

    // After canceling fails, the error toast should pop up.
    assertTrue(
        strictQuery('#toast', scanningApp.shadowRoot, CrToastElement).open);
    assertTrue(isVisible(
        strictQuery('#toastInfoIcon', scanningApp.shadowRoot, HTMLElement)));
    assertTrue(isVisible(
        strictQuery('#getHelpLink', scanningApp.shadowRoot, HTMLElement)));
    assertEquals(
        scanningApp.i18n('cancelFailedToastText'),
        strictQuery('#toastText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());
    // The scan progress page should still be showing with the cancel
    // button visible.
    const scanPreview =
        strictQuery('#scanPreview', scanningApp.shadowRoot, HTMLElement);
    assert(scanPreview);
    assertTrue(isVisible(
        strictQuery('#scanProgress', scanPreview.shadowRoot, HTMLElement)));
    assertTrue(isVisible(cancelButton));
    assertFalse(
        isVisible(scanPreview.shadowRoot!.querySelector('#helperText')));
    assertFalse(isVisible(scanButton));
  });

  // Verify the scan failed to start toast shows when a scan job fails to start.
  test('ScanFailedToStart', async () => {
    fakeScanService.setFailStartScan(true);

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    const toast = strictQuery('#toast', scanningApp.shadowRoot, CrToastElement);
    assertFalse(toast.open);
    // Click the Scan button and the scan will fail to start.
    scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement);
    scanButton!.click();
    await fakeScanService.whenCalled('startScan');

    assertTrue(toast.open);
    assertTrue(isVisible(
        strictQuery('#toastInfoIcon', scanningApp.shadowRoot, HTMLElement)));
    assertTrue(isVisible(
        strictQuery('#getHelpLink', scanningApp.shadowRoot, HTMLElement)));
    assertEquals(
        scanningApp.i18n('startScanFailedToast'),
        strictQuery('#toastText', scanningApp.shadowRoot, HTMLElement)
            .textContent!.trim());

    assertFalse(scanButton!.disabled);
    assertTrue(isVisible(scanButton));
  });

  // Verify the left and right panel exist on app initialization.
  test('PanelContainerContent', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    const panelContainer =
        strictQuery('#panelContainer', scanningApp.shadowRoot, HTMLElement);
    assertTrue(!!panelContainer);

    const leftPanel = strictQuery(
        '#panelContainer > #leftPanel', scanningApp.shadowRoot, HTMLElement);
    const rightPanel = strictQuery(
        '#panelContainer > #rightPanel', scanningApp.shadowRoot, HTMLElement);

    assertTrue(!!leftPanel);
    assertTrue(!!rightPanel);
  });

  // Verify the more settings toggle behavior.
  test('MoreSettingsToggle', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    await getScannerCapabilities();

    // Verify that expandable section is closed by default.
    assertFalse(isSettingsOpen());
    // Expand more settings section.
    await clickMoreSettingsButton();

    assertTrue(isSettingsOpen());
    // Close more settings section.
    await clickMoreSettingsButton();

    assertFalse(isSettingsOpen());
  });

  // Verify the loading page container is shown when no scanners are available.
  test('NoScanners', async () => {
    await initializeScanningApp(/*scanners=*/[], /*capabilities=*/ new Map());
    assert(scanningApp);
    assertTrue(isVisible(
        strictQuery('loading-page', scanningApp.shadowRoot, HTMLElement)));
    assertFalse(isVisible(
        strictQuery('#panelContainer', scanningApp.shadowRoot, HTMLElement)));
  });

  // Verify clicking the retry button loads available scanners.
  test('RetryClickLoadsScanners', async () => {
    await initializeScanningApp(/*scanners=*/[], /*capabilities=*/ new Map());
    assert(scanningApp);
    const loadingPage =
        strictQuery('loading-page', scanningApp.shadowRoot, HTMLElement);
    assertTrue(isVisible(loadingPage));
    const panelContainer =
        strictQuery('#panelContainer', scanningApp.shadowRoot, HTMLElement);
    assertFalse(isVisible(panelContainer));

    fakeScanService.setScanners(expectedScanners);
    fakeScanService.setCapabilities(capabilities);
    strictQuery('#retryButton', loadingPage.shadowRoot, CrButtonElement)
        .click();
    await fakeScanService.whenCalled('getScanners');

    assertFalse(isVisible(loadingPage));
    assertTrue(isVisible(panelContainer));
  });

  // Verify no changes are recorded when a scan job is initiated without any
  // settings changes.
  test('RecordNoSettingChanges', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await testBrowserProxy.whenCalled('recordNumScanSettingChanges');

    const numScanSettingChanges =
        testBrowserProxy.getArgs('recordNumScanSettingChanges')[0];
    assertEquals(0, numScanSettingChanges);
  });

  // Verify the correct amount of settings changes are recorded when a scan job
  // is initiated.
  test('RecordSomeSettingChanges', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    await changeSelectedValue(
        getSettingSelect('#fileTypeSelect'), FileType.kJpg.toString());

    await changeSelectedValue(getSettingSelect('#resolutionSelect'), '75');

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    await testBrowserProxy.whenCalled('recordNumScanSettingChanges');

    const numScanSettingChanges =
        testBrowserProxy.getArgs('recordNumScanSettingChanges')[0];
    assertEquals(2, numScanSettingChanges);
  });

  // Verify the correct amount of changes are recorded after the selected
  // scanner is changed then a scan job is initiated.
  test('RecordSettingsWithScannerChange', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    await changeSelectedValue(
        getSettingSelect('#colorModeSelect'),
        ColorMode.kBlackAndWhite.toString());

    await changeSelectedIndex(getSettingSelect('#scannerSelect'), /*index=*/ 1);

    await changeSelectedValue(
        getSettingSelect('#fileTypeSelect'), FileType.kJpg.toString());

    await changeSelectedValue(getSettingSelect('#resolutionSelect'), '150');

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();
    const numScanSettingChanges =
        testBrowserProxy.getArgs('recordNumScanSettingChanges')[0];
    assertEquals(3, numScanSettingChanges);
  });

  // Verify the default scan settings are chosen on app load.
  test('DefaultScanSettings', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    assertEquals(
        tokenToString(firstScannerId),
        getSettingSelect('#scannerSelect').value);
    assertEquals(PLATEN, getSettingSelect('#sourceSelect').value);
    assertEquals(
        loadTimeData.getString('myFilesSelectOption'),
        getSettingSelect('#scanToSelect').value);
    assertEquals(
        FileType.kPdf.toString(), getSettingSelect('#fileTypeSelect').value);
    assertEquals(
        ColorMode.kColor.toString(),
        getSettingSelect('#colorModeSelect').value);
    assertEquals(
        PageSize.kIsoA4.toString(), getSettingSelect('#pageSizeSelect').value);
    assertEquals('300', getSettingSelect('#resolutionSelect').value);
  });

  // Verify the first option in each settings dropdown is selected when the
  // default option is not available on the selected scanner.
  test('DefaultScanSettingsNotAvailable', async () => {
    await initializeScanningApp(expectedScanners.slice(1), capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    assertEquals(
        tokenToString(secondScannerId),
        getSettingSelect('#scannerSelect').value);
    assertEquals(ADF_SIMPLEX, getSettingSelect('#sourceSelect').value);
    assertEquals(
        loadTimeData.getString('myFilesSelectOption'),
        getSettingSelect('#scanToSelect').value);
    assertEquals(
        FileType.kPdf.toString(), getSettingSelect('#fileTypeSelect').value);
    assertEquals(
        ColorMode.kBlackAndWhite.toString(),
        getSettingSelect('#colorModeSelect').value);
    assertEquals(
        PageSize.kIsoA4.toString(), getSettingSelect('#pageSizeSelect').value);
    assertEquals('600', getSettingSelect('#resolutionSelect').value);
  });

  // Verify the default scan settings are used when saved settings are not
  // available for the selected scanner.
  test('SavedSettingsNotAvailable', async () => {
    const savedScanSettings = {
      lastUsedScannerName: 'Wrong Scanner',
      scanToPath: 'scan/to/path',
      scanners: [{
        name: 'Wrong Scanner',
        lastScanDate: new Date(),
        sourceName: ADF_DUPLEX,
        fileType: FileType.kPng,
        colorMode: ColorMode.kGrayscale,
        pageSize: PageSize.kMax,
        resolutionDpi: 100,
        multiPageScanChecked: false,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    // Set up from saved settings occurs after next render on app state change.
    await waitAfterNextRender(scanningApp);

    assertEquals(
        tokenToString(firstScannerId),
        getSettingSelect('#scannerSelect').value);
    assertEquals(PLATEN, getSettingSelect('#sourceSelect').value);
    assertEquals(
        loadTimeData.getString('myFilesSelectOption'),
        getSettingSelect('#scanToSelect').value);
    assertEquals(
        FileType.kPdf.toString(), getSettingSelect('#fileTypeSelect').value);
    assertEquals(
        ColorMode.kColor.toString(),
        getSettingSelect('#colorModeSelect').value);
    assertEquals(
        PageSize.kIsoA4.toString(), getSettingSelect('#pageSizeSelect').value);
    assertEquals('300', getSettingSelect('#resolutionSelect').value);
    assertFalse(scanningApp.multiPageScanChecked);
  });

  // Verify saved settings are applied when available for the selected scanner.
  test('ApplySavedSettings', async () => {
    const selectedPath = {baseName: 'path', filePath: 'valid/scan/to/path'};
    testBrowserProxy.setSavedSettingsSelectedPath(selectedPath);

    const savedScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: selectedPath.filePath,
      scanners: [{
        name: firstScannerName,
        lastScanDate: new Date(),
        sourceName: PLATEN,
        fileType: FileType.kPdf,
        colorMode: ColorMode.kBlackAndWhite,
        pageSize: PageSize.kMax,
        resolutionDpi: 75,
        multiPageScanChecked: true,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    // Set up from saved settings occurs after next render on app state change.
    await waitAfterNextRender(scanningApp);

    assertEquals(
        tokenToString(firstScannerId), getSettingSelect('#scannerSelect').value,
        'Scanner select');
    assertEquals(
        PLATEN, getSettingSelect('#sourceSelect').value, 'Source select');
    assertEquals(
        selectedPath.baseName, getSettingSelect('#scanToSelect').value,
        'Scan path');
    assertEquals(
        FileType.kPdf.toString(), getSettingSelect('#fileTypeSelect').value,
        'File type select');
    assertEquals(
        ColorMode.kBlackAndWhite.toString(),
        getSettingSelect('#colorModeSelect').value, 'Color mode select');
    assertEquals(
        PageSize.kMax.toString(), getSettingSelect('#pageSizeSelect').value,
        'Page size select');
    assertEquals(
        '75', getSettingSelect('#resolutionSelect').value, 'Resolution select');
    assertTrue(scanningApp.multiPageScanChecked, 'Multi-Page-Scan checked');
  });

  // Verify if the setting value stored in saved settings is no longer
  // available on the selected scanner, the default setting is chosen.
  test('SettingNotFoundInCapabilities', async () => {
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
        colorMode: ColorMode.kGrayscale,
        pageSize: -1,
        resolutionDpi: 600,
        multiPageScanChecked: false,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    assertEquals(
        tokenToString(firstScannerId),
        getSettingSelect('#scannerSelect').value);
    assertEquals(PLATEN, getSettingSelect('#sourceSelect').value);
    assertEquals(
        loadTimeData.getString('myFilesSelectOption'),
        getSettingSelect('#scanToSelect').value);
    assertEquals(
        FileType.kPdf.toString(), getSettingSelect('#fileTypeSelect').value);
    assertEquals(
        ColorMode.kColor.toString(),
        getSettingSelect('#colorModeSelect').value);
    assertEquals(
        PageSize.kIsoA4.toString(), getSettingSelect('#pageSizeSelect').value);
    assertEquals('300', getSettingSelect('#resolutionSelect').value);
    assertFalse(scanningApp.multiPageScanChecked);
  });

  // Verify if |multiPageScanChecked| is true in saved settings but the
  // scanner's capabilities doesn't support it, the multi-page scan checkbox
  // will not be set.
  test('MultiPageNotAvailableFromCapabilities', async () => {
    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: '',
      scanners: [{
        name: secondScannerName,
        lastScanDate: new Date(),
        sourceName: PLATEN,
        fileType: FileType.kPdf,
        colorMode: ColorMode.kGrayscale,
        pageSize: PageSize.kNaLetter,
        resolutionDpi: 600,
        multiPageScanChecked: true,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    // `secondScanner` does not have PLATEN in it's capabilities so the
    // multi-page scan checkbox should not get set.
    assertFalse(scanningApp.multiPageScanChecked);
  });

  // Verify if the |multiPageScanChecked| is not present in the saved settings
  // JSON (i.e. the first time the feature is enabled), the multi-page scan
  // checkbox will not be set.
  test('MultiPageNotInSavedSettings', async () => {
    const savedScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: '',
      scanners: [{
        name: secondScannerName,
        lastScanDate: new Date(),
        sourceName: PLATEN,
        fileType: FileType.kPdf,
        colorMode: ColorMode.kGrayscale,
        pageSize: PageSize.kNaLetter,
        resolutionDpi: 600,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    // The multi-page scan checkbox should not get set because it wasn't
    // present in the saved settings.
    assertFalse(scanningApp.multiPageScanChecked);
  });

  // Verify the last used scanner is selected from saved settings.
  test('selectLastUsedScanner', async () => {
    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: 'scan/to/path',
      scanners: [{
        name: secondScannerName,
        lastScanDate: new Date(),
        sourceName: ADF_DUPLEX,
        fileType: FileType.kPng,
        colorMode: ColorMode.kBlackAndWhite,
        pageSize: PageSize.kMax,
        resolutionDpi: 75,
        multiPageScanChecked: false,
      }],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    const scannerSelect = getSettingSelect('#scannerSelect');
    assertEquals(tokenToString(secondScannerId), scannerSelect.value);
  });

  // Verify the scan settings are sent to the Pref service to be saved.
  test('saveScanSettings', async () => {
    const scannerSetting = {
      name: secondScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_DUPLEX,
      fileType: FileType.kPng,
      colorMode: ColorMode.kBlackAndWhite,
      pageSize: PageSize.kMax,
      resolutionDpi: 200,
      multiPageScanChecked: false,
    };
    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: [scannerSetting],
    };
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    // Set dropdowns to match `scannerSettings` properties.
    await changeSelectedValue(
        getSettingSelect('#scannerSelect'), tokenToString(secondScannerId));
    await changeSelectedValue(
        getSettingSelect('#sourceSelect'),
        scannerSetting.sourceName.toString());
    await changeSelectedValue(
        getSettingSelect('#fileTypeSelect'),
        scannerSetting.fileType.toString());
    await changeSelectedValue(
        getSettingSelect('#colorModeSelect'),
        scannerSetting.colorMode.toString());
    await changeSelectedValue(
        getSettingSelect('#pageSizeSelect'),
        scannerSetting.pageSize.toString());
    await changeSelectedValue(
        getSettingSelect('#resolutionSelect'),
        scannerSetting.resolutionDpi.toString());

    assertEquals(
        scannerSetting.sourceName.toString(), scanningApp.selectedSource);
    assertEquals(
        scannerSetting.fileType.toString(), scanningApp.selectedFileType);
    assertEquals(
        scannerSetting.colorMode.toString(), scanningApp.selectedColorMode);
    assertEquals(
        scannerSetting.pageSize.toString(), scanningApp.selectedPageSize);
    assertEquals(
        scannerSetting.resolutionDpi.toString(),
        scanningApp.selectedResolution);

    strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement).click();
    await testBrowserProxy.whenCalled('saveScanSettings');

    const actualSaveScanSettingsArg =
        testBrowserProxy.getArgs('saveScanSettings')[0];
    assert(actualSaveScanSettingsArg);
    const actualSavedScanSettings = JSON.parse(actualSaveScanSettingsArg);
    compareSavedScanSettings(savedScanSettings, actualSavedScanSettings);
  });

  // Verify that the correct scanner setting is replaced when saving scan
  // settings to the Pref service.
  test('replaceExistingScannerInScanSettings', async () => {
    const firstScannerSetting = {
      name: firstScannerName,
      lastScanDate: LAST_SCAN_DATE,
      sourceName: ADF_DUPLEX,
      fileType: FileType.kPng,
      colorMode: ColorMode.kBlackAndWhite,
      pageSize: PageSize.kMax,
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
      fileType: FileType.kPng,
      colorMode: ColorMode.kBlackAndWhite,
      pageSize: PageSize.kMax,
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
      fileType: FileType.kJpg,
      colorMode: ColorMode.kGrayscale,
      pageSize: PageSize.kIsoA4,
      resolutionDpi: 600,
      multiPageScanChecked: false,
    };
    savedScanSettings.scanners[1] = newSecondScannerSetting;

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();

    scanningApp.selectedScannerId = tokenToString(secondScannerId);
    scanningApp.selectedSource = newSecondScannerSetting.sourceName;
    scanningApp.selectedFileType = newSecondScannerSetting.fileType.toString();
    scanningApp.selectedColorMode =
        newSecondScannerSetting.colorMode.toString();
    scanningApp.selectedPageSize = newSecondScannerSetting.pageSize.toString();
    scanningApp.selectedResolution =
        newSecondScannerSetting.resolutionDpi.toString();

    strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement).click();

    const actualSavedScanSettings =
        JSON.parse(testBrowserProxy.getArgs('saveScanSettings')[0]);
    compareSavedScanSettings(savedScanSettings, actualSavedScanSettings);
  });

  // Verify that the correct scanner gets evicted when there are too many
  // scanners in saved scan settings.
  test('evictScannersOverTheMaxLimit', async () => {
    const scannerToEvict = {
      name: secondScannerName,
      lastScanDate: '1/1/2021',
      sourceName: ADF_DUPLEX,
      fileType: FileType.kPng,
      colorMode: ColorMode.kBlackAndWhite,
      pageSize: PageSize.kMax,
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

    const scannersToKeep: Scanner[] =
        new Array(MAX_NUM_SAVED_SCANNERS).fill(scannerToKeep);

    // Put |scannerToEvict| in the front of |scannersToKeep| to test that it
    // get correctly sorted to the back of the array when evicting scanners.
    const savedScanSettings = {
      lastUsedScannerName: secondScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: [scannerToEvict, ...scannersToKeep],
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();
    await flushTasks();
    strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement).click();
    await flushTasks();

    const actualSavedScanSettings =
        JSON.parse(testBrowserProxy.getArgs('saveScanSettings')[0]);
    assertEquals(
        MAX_NUM_SAVED_SCANNERS, actualSavedScanSettings.scanners.length);
    assertArrayEquals(scannersToKeep, actualSavedScanSettings.scanners);
  });

  // Verify that no scanners get evicted when the number of scanners in saved
  // scan settings is equal to |MAX_NUM_SAVED_SCANNERS|.
  test('doNotEvictScannersAtMax', async () => {
    const scanners: ScannerSetting[] = new Array(MAX_NUM_SAVED_SCANNERS);
    for (let i = 0; i < MAX_NUM_SAVED_SCANNERS; i++) {
      scanners[i] = {
        name: 'Scanner ' + (i + 1),
        lastScanDate: new Date(new Date().getTime() + i),
        sourceName: ADF_DUPLEX,
        fileType: FileType.kPng,
        colorMode: ColorMode.kBlackAndWhite,
        pageSize: PageSize.kMax,
        resolutionDpi: 300,
        multiPageScanChecked: false,
      };
    }
    const savedScanSettings: ScanSettings = {
      lastUsedScannerName: firstScannerName,
      scanToPath: MY_FILES_PATH,
      scanners: scanners,
    };
    testBrowserProxy.setSavedSettings(JSON.stringify(savedScanSettings));

    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();
    await waitAfterNextRender(scanningApp);
    strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement).click();
    const saveScanSettingsFn = 'saveScanSettings';
    await testBrowserProxy.whenCalled(saveScanSettingsFn);

    assertEquals(1, testBrowserProxy.getCallCount(saveScanSettingsFn));
    const actualSavedScanSettings =
        JSON.parse(testBrowserProxy.getArgs(saveScanSettingsFn)[0]);
    assertEquals(
        MAX_NUM_SAVED_SCANNERS, actualSavedScanSettings.scanners.length);
    compareSavedScanSettings(savedScanSettings, actualSavedScanSettings);
  });

  // Verify that the multi-page scanning checkbox is only visible when both
  // Flatbed and PDF scan settings are selected.
  test('showMultiPageCheckbox', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();
    scanningApp.selectedSource = ADF_DUPLEX;
    scanningApp.selectedFileType = FileType.kPng.toString();
    await waitAfterNextRender(scanningApp);

    const multiBoxCheckbox =
        strictQuery('multi-page-checkbox', scanningApp.shadowRoot, HTMLElement);
    assert(multiBoxCheckbox);
    const checkboxDiv =
        strictQuery('#checkboxDiv', multiBoxCheckbox.shadowRoot, HTMLElement);
    assertFalse(isVisible(checkboxDiv));

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPng.toString();
    await waitAfterNextRender(scanningApp);

    assertFalse(isVisible(checkboxDiv));

    scanningApp.selectedSource = ADF_DUPLEX;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await waitAfterNextRender(scanningApp);

    assertFalse(isVisible(checkboxDiv));

    scanningApp.selectedSource = PLATEN;
    scanningApp.selectedFileType = FileType.kPdf.toString();
    await waitAfterNextRender(scanningApp);

    assertTrue(isVisible(checkboxDiv));
  });

  // Verify a normal scan is started when the multi-page checkbox is checked
  // while a non-PDF file type is selected.
  test('OnlyMultiPageScanWhenPDFIsSelected', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();
    await waitAfterNextRender(scanningApp);
    const sourceSelect = getSettingSelect('#sourceSelect');
    const sourceChange = eventToPromise('change', sourceSelect);
    await changeSelectedValue(sourceSelect, PLATEN);
    await sourceChange;
    const fileSelect = getSettingSelect('#fileTypeSelect');
    const fileChange = eventToPromise('change', fileSelect);
    await changeSelectedValue(fileSelect, FileType.kPdf.toString());
    await fileChange;
    await ensureMultiPageScanCheckboxChecked(/*checked=*/ true);

    const scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, CrButtonElement);
    assert(scanButton);
    assertEquals('Scan page 1', scanButton.textContent!.trim());

    // Leave the multi-page checkbox checked but switch the file type.
    scanningApp.selectedFileType = FileType.kPng.toString();
    await flushTasks();

    assertEquals('Scan', scanButton.textContent!.trim());
    // When scan button is clicked expect a normal scan to start.
    scanButton.click();

    await fakeScanService.whenCalled('startScan');
  });

  // Verify a normal scan is started when the multi-page checkbox is checked
  // while a non-Flatbed source type is selected.
  test('OnlyMultiPageScanWhenFlatbedIsSelected', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();
    await waitAfterNextRender(scanningApp);
    await changeSelectedValue(getSettingSelect('#sourceSelect'), PLATEN);
    await changeSelectedValue(
        getSettingSelect('#fileTypeSelect'), FileType.kPdf.toString());
    await ensureMultiPageScanCheckboxChecked(/*checked=*/ true);

    const scanButton =
        strictQuery('#scanButton', scanningApp.shadowRoot, HTMLElement);
    assert(scanButton);
    assertEquals('Scan page 1', scanButton.textContent!.trim());

    // Leave the multi-page checkbox checked but switch the source.
    scanningApp.selectedSource = ADF_SIMPLEX;
    await flushTasks();

    assertEquals('Scan', scanButton.textContent!.trim());

    // When scan button is clicked expect a normal scan to start.
    scanButton!.click();
    await fakeScanService.whenCalled('startScan');
  });

  // Verify the scan settings update according to the source selected.
  test('UpdateSettingsBySource', async () => {
    await initializeScanningApp(expectedScanners, capabilities);
    assert(scanningApp);
    await getScannerCapabilities();
    scanningApp.selectedSource = PLATEN;
    await waitAfterNextRender(scanningApp);

    const pageSize =
        strictQuery('#pageSizeSelect', scanningApp.shadowRoot, HTMLElement);
    assert(pageSize);
    const pageSizeSelect = pageSize.shadowRoot!.querySelector('select')!;
    changeSelectedValue(pageSizeSelect, PageSize.kIsoA4.toString());
    assertEquals(PageSize.kIsoA4.toString(), pageSizeSelect.value);
    changeSelectedValue(pageSizeSelect, PageSize.kMax.toString());
    assertEquals(PageSize.kMax.toString(), pageSizeSelect.value);

    scanningApp.selectedSource = ADF_DUPLEX;
    await waitAfterNextRender(scanningApp);

    changeSelectedValue(pageSizeSelect, PageSize.kIsoA4.toString());
    assertEquals(PageSize.kIsoA4.toString(), pageSizeSelect.value);
    changeSelectedValue(pageSizeSelect, PageSize.kNaLetter.toString());
    assertEquals(PageSize.kNaLetter.toString(), pageSizeSelect.value);
    changeSelectedValue(pageSizeSelect, PageSize.kMax.toString());
    assertEquals(PageSize.kMax.toString(), pageSizeSelect.value);
  });
});  // End of suite
