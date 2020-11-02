// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jschettler): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://scanning/scan_preview.js';
import 'chrome://scanning/scanning_app.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {setScanServiceForTesting} from 'chrome://scanning/mojo_interface_provider.js';
import {ScannerArr} from 'chrome://scanning/scanning_app_types.js';
import {getColorModeString, getPageSizeString, getSourceTypeString, tokenToString} from 'chrome://scanning/scanning_app_util.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';

import {flushTasks} from '../../test_util.m.js';

import * as utils from './scanning_app_test_utils.js';
import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

const ColorMode = {
  BLACK_AND_WHITE: chromeos.scanning.mojom.ColorMode.kBlackAndWhite,
  GRAYSCALE: chromeos.scanning.mojom.ColorMode.kGrayscale,
  COLOR: chromeos.scanning.mojom.ColorMode.kColor,
};

const FileType = {
  JPG: chromeos.scanning.mojom.FileType.kJpg,
  PDF: chromeos.scanning.mojom.FileType.kPdf,
  PNG: chromeos.scanning.mojom.FileType.kPng,
};

const PageSize = {
  A4: chromeos.scanning.mojom.PageSize.kIsoA4,
  Letter: chromeos.scanning.mojom.PageSize.kNaLetter,
  Max: chromeos.scanning.mojom.PageSize.kMax,
};

const SourceType = {
  FLATBED: chromeos.scanning.mojom.SourceType.kFlatbed,
  ADF_SIMPLEX: chromeos.scanning.mojom.SourceType.kAdfSimplex,
  ADF_DUPLEX: chromeos.scanning.mojom.SourceType.kAdfDuplex,
};

const pageSizes = [PageSize.A4, PageSize.Letter, PageSize.Max];

class FakeScanService {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /** @private {!ScannerArr} */
    this.scanners_ = [];

    /**
     * @private {!Map<!mojoBase.mojom.UnguessableToken,
     *     !chromeos.scanning.mojom.ScannerCapabilities>}
     */
    this.capabilities_ = new Map();

    this.resetForTest();
  }

  resetForTest() {
    this.scanners_ = [];
    this.capabilities_ = new Map();
    this.resolverMap_.set('getScanners', new PromiseResolver());
    this.resolverMap_.set('getScannerCapabilities', new PromiseResolver());
    this.resolverMap_.set('scan', new PromiseResolver());
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    let method = this.resolverMap_.get(methodName);
    assert(!!method, `Method '${methodName}' not found.`);
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

  /** @param {chromeos.scanning.mojom.Scanner} scanner */
  addScanner(scanner) {
    this.scanners_ = this.scanners_.concat(scanner);
  }

  /**
   * @param {!Map<!mojoBase.mojom.UnguessableToken,
   *     !chromeos.scanning.mojom.ScannerCapabilities>} capabilities
   */
  setCapabilities(capabilities) {
    this.capabilities_ = capabilities;
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
   *    !chromeos.scanning.mojom.ScannerCapabilities}>}
   */
  getScannerCapabilities(scanner_id) {
    return new Promise(resolve => {
      this.methodCalled('getScannerCapabilities');
      resolve({capabilities: this.capabilities_.get(scanner_id)});
    });
  }

  /**
   * @param {!mojoBase.mojom.UnguessableToken} scanner_id
   * @param {!chromeos.scanning.mojom.ScanSettings} settings
   * @return {!Promise<{success: boolean}>}
   */
  scan(scanner_id, settings) {
    return new Promise(resolve => {
      this.methodCalled('scan');
      resolve({success: true});
    });
  }
}

suite('ScanningAppTest', () => {
  /** @type {?ScanningAppElement} */
  let scanningApp = null;

  /** @type {?chromeos.scanning.mojom.ScanServiceRemote} */
  let fakeScanService_;

  suiteSetup(() => {
    fakeScanService_ = new FakeScanService();
    setScanServiceForTesting(fakeScanService_);
  });

  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    fakeScanService_.resetForTest();
    scanningApp.remove();
    scanningApp = null;
  });

  /**
   * @param {!ScannerArr} scanners
   * @param {!Map<!mojoBase.mojom.UnguessableToken,
   *     !chromeos.scanning.mojom.ScannerCapabilities>} capabilities
   * @return {!Promise}
   */
  function initializeScanningApp(scanners, capabilities) {
    fakeScanService_.setScanners(scanners);
    fakeScanService_.setCapabilities(capabilities);
    scanningApp = document.createElement('scanning-app');
    document.body.appendChild(scanningApp);
    assert(!!scanningApp);
    return fakeScanService_.whenCalled('getScanners');
  }

  /**
   * Returns the "More settings" button.
   * @return {!CrButtonElement}
   */
  function getMoreSettingsButton() {
    const button = scanningApp.$$('#moreSettingsButton');
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
   * Returns whether the "More settings" section is expanded or not.
   * @return {boolean}
   */
  function isSettingsOpen() {
    return scanningApp.$.collapse.opened;
  }

  test('Scan', () => {
    const firstScannerId = {high: 0, low: 1};
    const firstScannerName = 'Scanner 1';
    const secondScannerId = {high: 0, low: 2};
    const secondScannerName = 'Scanner 2';
    const expectedScanners = [
      utils.createScanner(firstScannerId, firstScannerName),
      utils.createScanner(secondScannerId, secondScannerName)
    ];

    const firstCapabilities = {
      sources: [
        utils.createScannerSource(SourceType.FLATBED, 'platen', pageSizes),
        utils.createScannerSource(
            SourceType.ADF_DUPLEX, 'adf duplex', pageSizes),
      ],
      colorModes: [ColorMode.BLACK_AND_WHITE, ColorMode.COLOR],
      resolutions: [75, 100, 300]
    };
    const secondCapabilities = {
      sources: [utils.createScannerSource(
          SourceType.ADF_SIMPLEX, 'adf simplex', pageSizes)],
      colorModes: [ColorMode.GRAYSCALE],
      resolutions: [150, 600]
    };

    let capabilities = new Map();
    capabilities.set(firstScannerId, firstCapabilities);
    capabilities.set(secondScannerId, secondCapabilities);

    return initializeScanningApp(expectedScanners, capabilities)
        .then(() => {
          return fakeScanService_.whenCalled('getScannerCapabilities');
        })
        .then(() => {
          assertEquals(
              tokenToString(firstScannerId), scanningApp.selectedScannerId);
          assertEquals(
              firstCapabilities.sources[0].name, scanningApp.selectedSource);
          assertEquals(FileType.PNG.toString(), scanningApp.selectedFileType);
          assertEquals(
              firstCapabilities.colorModes[0].toString(),
              scanningApp.selectedColorMode);
          assertEquals(
              firstCapabilities.sources[0].pageSizes[0].toString(),
              scanningApp.selectedPageSize);
          assertEquals(
              firstCapabilities.resolutions[0].toString(),
              scanningApp.selectedResolution);

          // Before the scan button is clicked, the settings and scan button
          // should be enabled, and there should be no scan status.
          const scannerSelect = scanningApp.$$('#scannerSelect').$$('select');
          assertFalse(scannerSelect.disabled);
          const sourceSelect = scanningApp.$$('#sourceSelect').$$('select');
          assertFalse(sourceSelect.disabled);
          const fileTypeSelect = scanningApp.$$('#fileTypeSelect').$$('select');
          assertFalse(fileTypeSelect.disabled);
          const colorModeSelect =
              scanningApp.$$('#colorModeSelect').$$('select');
          assertFalse(colorModeSelect.disabled);
          const pageSizeSelect = scanningApp.$$('#pageSizeSelect').$$('select');
          assertFalse(pageSizeSelect.disabled);
          const resolutionSelect =
              scanningApp.$$('#resolutionSelect').$$('select');
          assertFalse(resolutionSelect.disabled);
          const scanButton = scanningApp.$$('#scanButton');
          assertFalse(scanButton.disabled);
          const statusText = scanningApp.$$('#statusText');
          assertEquals('', statusText.textContent.trim());

          // PNG is currently the only supported file type.
          fileTypeSelect.value = FileType.PNG.toString();
          fileTypeSelect.dispatchEvent(new CustomEvent('change'));
          flush();
          scanButton.click();

          // After the scan button is clicked, the settings and scan button
          // should be disabled, and the scan status should indicate that
          // scanning is in progress.
          assertTrue(scannerSelect.disabled);
          assertTrue(sourceSelect.disabled);
          assertTrue(fileTypeSelect.disabled);
          assertTrue(colorModeSelect.disabled);
          assertTrue(pageSizeSelect.disabled);
          assertTrue(resolutionSelect.disabled);
          assertTrue(scanButton.disabled);
          assertEquals('Scanning...', statusText.textContent.trim());
          return fakeScanService_.whenCalled('scan');
        })
        .then(() => {
          // After scanning is complete, the settings and scan button should be
          // enabled, and the scan status should indicate that scanning is
          // complete.
          assertFalse(scanningApp.$$('#scannerSelect').$$('select').disabled);
          assertFalse(scanningApp.$$('#sourceSelect').$$('select').disabled);
          assertFalse(scanningApp.$$('#fileTypeSelect').$$('select').disabled);
          assertFalse(scanningApp.$$('#colorModeSelect').$$('select').disabled);
          assertFalse(scanningApp.$$('#pageSizeSelect').$$('select').disabled);
          assertFalse(
              scanningApp.$$('#resolutionSelect').$$('select').disabled);
          assertFalse(scanningApp.$$('#scanButton').disabled);
          assertEquals(
              'Scan complete! File(s) saved to /home/chronos/user/MyFiles.',
              scanningApp.$$('#statusText').textContent.trim());
        });
  });

  test('PanelContainerContent', () => {
    const scanners = [];
    const capabilities = new Map();
    return initializeScanningApp(scanners, capabilities).then(() => {
      const panelContainer = scanningApp.$$('.panel-container');
      assertTrue(!!panelContainer);

      const leftPanel = scanningApp.$$('.panel-container > #leftPanel');
      const rightPanel = scanningApp.$$('.panel-container > #rightPanel');

      assertTrue(!!leftPanel);
      assertTrue(!!rightPanel);
    });
  });

  test('MoreSettingsToggle', () => {
    const scanners = [];
    const capabilities = new Map();
    return initializeScanningApp(scanners, capabilities)
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
});

suite('ScannerSelectTest', () => {
  /** @type {!ScannerSelectElement} */
  let scannerSelect;

  setup(() => {
    scannerSelect = document.createElement('scanner-select');
    assertTrue(!!scannerSelect);
    scannerSelect.loaded = false;
    document.body.appendChild(scannerSelect);
  });

  teardown(() => {
    scannerSelect.remove();
    scannerSelect = null;
  });

  test('initializeScannerSelect', () => {
    // Before options are added, the dropdown should be hidden and the throbber
    // should be visible.
    const select = scannerSelect.$$('select');
    assertTrue(!!select);
    assertTrue(select.hidden);
    const throbber = scannerSelect.$$('.throbber-container');
    assertTrue(!!throbber);
    assertFalse(throbber.hidden);

    const firstScannerId = {high: 0, low: 1};
    const firstScannerName = 'Scanner 1';
    const secondScannerId = {high: 0, low: 2};
    const secondScannerName = 'Scanner 2';
    const scannerArr = [
      utils.createScanner(firstScannerId, firstScannerName),
      utils.createScanner(secondScannerId, secondScannerName)
    ];
    scannerSelect.scanners = scannerArr;
    scannerSelect.loaded = true;
    flush();

    // Verify that adding scanners and setting loaded to true results in the
    // dropdown becoming visible with the correct options.
    assertFalse(select.disabled);
    assertFalse(select.hidden);
    assertTrue(throbber.hidden);
    assertEquals(2, select.length);
    assertEquals(firstScannerName, select.options[0].textContent.trim());
    assertEquals(secondScannerName, select.options[1].textContent.trim());
    assertEquals(tokenToString(firstScannerId), select.value);
  });

  test('scannerSelectDisabled', () => {
    const select = scannerSelect.$$('select');
    assertTrue(!!select);

    let scannerArr = [utils.createScanner({high: 0, low: 1}, 'Scanner 1')];
    scannerSelect.scanners = scannerArr;
    scannerSelect.loaded = true;
    flush();

    // Verify the dropdown is disabled when there's only one option.
    assertEquals(1, select.length);
    assertTrue(select.disabled);

    scannerArr = scannerArr.concat(
        [utils.createScanner({high: 0, low: 2}, 'Scanner 2')]);
    scannerSelect.scanners = scannerArr;
    flush();

    // Verify the dropdown is enabled when there's more than one option.
    assertEquals(2, select.length);
    assertFalse(select.disabled);
  });

  test('noScanners', () => {
    const select = scannerSelect.$$('select');
    assertTrue(!!select);

    scannerSelect.loaded = true;
    flush();

    // Verify the dropdown is disabled and displays the default option when no
    // scanners are available.
    assertEquals(1, select.length);
    assertEquals('No available scanners', select.options[0].textContent.trim());
    assertTrue(select.disabled);
  });
});

suite('SourceSelectTest', () => {
  /** @type {!SourceSelectElement} */
  let sourceSelect;

  setup(() => {
    sourceSelect = document.createElement('source-select');
    assertTrue(!!sourceSelect);
    document.body.appendChild(sourceSelect);
  });

  teardown(() => {
    sourceSelect.remove();
    sourceSelect = null;
  });

  test('initializeSourceSelect', () => {
    // Before options are added, the dropdown should be disabled and empty.
    const select = sourceSelect.$$('select');
    assertTrue(!!select);
    assertTrue(select.disabled);
    assertEquals(0, select.length);

    const firstSource =
        utils.createScannerSource(SourceType.FLATBED, 'platen', pageSizes);
    const secondSource = utils.createScannerSource(
        SourceType.ADF_SIMPLEX, 'adf simplex', pageSizes);
    const sourceArr = [firstSource, secondSource];
    sourceSelect.sources = sourceArr;
    flush();

    // Verify that adding more than one source results in the dropdown becoming
    // enabled with the correct options.
    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(
        getSourceTypeString(firstSource.type),
        select.options[0].textContent.trim());
    assertEquals(
        getSourceTypeString(secondSource.type),
        select.options[1].textContent.trim());
    assertEquals(firstSource.name, select.value);
  });

  test('sourceSelectDisabled', () => {
    const select = sourceSelect.$$('select');
    assertTrue(!!select);

    let sourceArr =
        [utils.createScannerSource(SourceType.FLATBED, 'flatbed', pageSizes)];
    sourceSelect.sources = sourceArr;
    flush();

    // Verify the dropdown is disabled when there's only one option.
    assertEquals(1, select.length);
    assertTrue(select.disabled);

    sourceArr = sourceArr.concat([utils.createScannerSource(
        SourceType.ADF_DUPLEX, 'adf duplex', pageSizes)]);
    sourceSelect.sources = sourceArr;
    flush();

    // Verify the dropdown is enabled when there's more than one option.
    assertEquals(2, select.length);
    assertFalse(select.disabled);
  });
});

suite('FileTypeSelectTest', () => {
  /** @type {!FileTypeSelectElement} */
  let fileTypeSelect;

  setup(() => {
    fileTypeSelect = document.createElement('file-type-select');
    assertTrue(!!fileTypeSelect);
    document.body.appendChild(fileTypeSelect);
  });

  teardown(() => {
    fileTypeSelect.remove();
    fileTypeSelect = null;
  });

  test('initializeFileTypeSelect', () => {
    // The dropdown should be initialized as enabled with three options. The
    // default option should be PNG.
    const select = fileTypeSelect.$$('select');
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(3, select.length);
    assertEquals('JPG', select.options[0].textContent.trim());
    assertEquals('PDF', select.options[1].textContent.trim());
    assertEquals('PNG', select.options[2].textContent.trim());
    assertEquals(FileType.PNG.toString(), select.value);

    // Selecting a different option should update the selected value.
    select.value = FileType.JPG.toString();
    select.dispatchEvent(new CustomEvent('change'));
    flush();

    assertEquals(FileType.JPG.toString(), fileTypeSelect.selectedFileType);
  });
});

suite('ColorModeSelectTest', () => {
  /** @type {!ColorModeSelectElement} */
  let colorModeSelect;

  setup(() => {
    colorModeSelect = document.createElement('color-mode-select');
    assertTrue(!!colorModeSelect);
    document.body.appendChild(colorModeSelect);
  });

  teardown(() => {
    colorModeSelect.remove();
    colorModeSelect = null;
  });

  test('initializeColorModeSelect', () => {
    // Before options are added, the dropdown should be disabled and empty.
    const select = colorModeSelect.$$('select');
    assertTrue(!!select);
    assertTrue(select.disabled);
    assertEquals(0, select.length);

    const firstColorMode = ColorMode.COLOR;
    const secondColorMode = ColorMode.GRAYSCALE;
    colorModeSelect.colorModes = [firstColorMode, secondColorMode];
    flush();

    // Verify that adding more than one color mode results in the dropdown
    // becoming enabled with the correct options.
    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(
        getColorModeString(firstColorMode),
        select.options[0].textContent.trim());
    assertEquals(
        getColorModeString(secondColorMode),
        select.options[1].textContent.trim());
    assertEquals(firstColorMode.toString(), select.value);
  });

  test('colorModeSelectDisabled', () => {
    const select = colorModeSelect.$$('select');
    assertTrue(!!select);

    let colorModeArr = [ColorMode.BLACK_AND_WHITE];
    colorModeSelect.colorModes = colorModeArr;
    flush();

    // Verify the dropdown is disabled when there's only one option.
    assertEquals(1, select.length);
    assertTrue(select.disabled);

    colorModeArr = colorModeArr.concat([ColorMode.GRAYSCALE]);
    colorModeSelect.colorModes = colorModeArr;
    flush();

    // Verify the dropdown is enabled when there's more than one option.
    assertEquals(2, select.length);
    assertFalse(select.disabled);
  });
});

suite('PageSizeSelectTest', () => {
  /** @type {!PageSizeSelectElement} */
  let pageSizeSelect;

  setup(() => {
    pageSizeSelect = document.createElement('page-size-select');
    assertTrue(!!pageSizeSelect);
    document.body.appendChild(pageSizeSelect);
  });

  teardown(() => {
    pageSizeSelect.remove();
    pageSizeSelect = null;
  });

  test('initializePageSizeSelect', () => {
    // Before options are added, the dropdown should be disabled and empty.
    const select = pageSizeSelect.$$('select');
    assertTrue(!!select);
    assertTrue(select.disabled);
    assertEquals(0, select.length);

    const firstPageSize = PageSize.A4;
    const secondPageSize = PageSize.Max;
    pageSizeSelect.pageSizes = [firstPageSize, secondPageSize];
    flush();

    // Verify that adding more than one page size results in the dropdown
    // becoming enabled with the correct options.
    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(
        getPageSizeString(firstPageSize), select.options[0].textContent.trim());
    assertEquals(
        getPageSizeString(secondPageSize),
        select.options[1].textContent.trim());
    assertEquals(firstPageSize.toString(), select.value);

    // Selecting a different option should update the selected value.
    select.value = secondPageSize.toString();
    select.dispatchEvent(new CustomEvent('change'));
    flush();

    assertEquals(secondPageSize.toString(), pageSizeSelect.selectedPageSize);
  });

  test('pageSizeSelectDisabled', () => {
    const select = pageSizeSelect.$$('select');
    assertTrue(!!select);

    let pageSizeArr = [PageSize.Letter];
    pageSizeSelect.pageSizes = pageSizeArr;
    flush();

    // Verify the dropdown is disabled when there's only one option.
    assertEquals(1, select.length);
    assertTrue(select.disabled);

    pageSizeArr = pageSizeArr.concat([PageSize.A4]);
    pageSizeSelect.pageSizes = pageSizeArr;
    flush();

    // Verify the dropdown is enabled when there's more than one option.
    assertEquals(2, select.length);
    assertFalse(select.disabled);
  });
});

suite('ResolutionSelectTest', () => {
  /** @type {!ResolutionSelectElement} */
  let resolutionSelect;

  setup(() => {
    resolutionSelect = document.createElement('resolution-select');
    assertTrue(!!resolutionSelect);
    document.body.appendChild(resolutionSelect);
  });

  teardown(() => {
    resolutionSelect.remove();
    resolutionSelect = null;
  });

  test('initializeResolutionSelect', () => {
    // Before options are added, the dropdown should be disabled and empty.
    const select = resolutionSelect.$$('select');
    assertTrue(!!select);
    assertTrue(select.disabled);
    assertEquals(0, select.length);

    const firstResolution = 75;
    const secondResolution = 300;
    resolutionSelect.resolutions = [firstResolution, secondResolution];
    flush();

    // Verify that adding more than one resolution results in the dropdown
    // becoming enabled with the correct options.
    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(
        firstResolution.toString() + ' dpi',
        select.options[0].textContent.trim());
    assertEquals(
        secondResolution.toString() + ' dpi',
        select.options[1].textContent.trim());
    assertEquals(firstResolution.toString(), select.value);

    // Selecting a different option should update the selected value.
    select.value = secondResolution;
    select.dispatchEvent(new CustomEvent('change'));
    flush();

    assertEquals(
        secondResolution.toString(), resolutionSelect.selectedResolution);
  });

  test('resolutionSelectDisabled', () => {
    const select = resolutionSelect.$$('select');
    assertTrue(!!select);

    let resolutionArr = [75];
    resolutionSelect.resolutions = resolutionArr;
    flush();

    // Verify the dropdown is disabled when there's only one option.
    assertEquals(1, select.length);
    assertTrue(select.disabled);

    resolutionArr = resolutionArr.concat([150]);
    resolutionSelect.resolutions = resolutionArr;
    flush();

    // Verify the dropdown is enabled when there's more than one option.
    assertEquals(2, select.length);
    assertFalse(select.disabled);
  });
});

suite('ScanPreviewTest', () => {
  /** @type {?ScanPreviewElement} */
  let scanPreview = null;

  setup(() => {
    scanPreview = document.createElement('scan-preview');
    assertTrue(!!scanPreview);
    document.body.appendChild(scanPreview);
  });

  teardown(() => {
    if (scanPreview) {
      scanPreview.remove();
    }
    scanPreview = null;
  });

  test('initializeScanPreview', () => {
    assertTrue(!!scanPreview.$$('.preview'));
  });
});

suite('ScanToSelectTest', () => {
  /** @type {?ScanToSelectElement} */
  let scanToSelect = null;

  /** @type {?TestScanningBrowserProxy} */
  let scanningBrowserProxy = null;

  /** @const {string} */
  const myFiles = 'My files';

  /** @const {string} */
  const selectFolderText = 'Select folder in Files app…';

  setup(() => {
    scanningBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.instance_ = scanningBrowserProxy;

    scanToSelect = document.createElement('scan-to-select');
    assertTrue(!!scanToSelect);
    document.body.appendChild(scanToSelect);
  });

  teardown(() => {
    if (scanToSelect) {
      scanToSelect.remove();
    }
    scanToSelect = null;
  });

  test('initializeScanToSelect', () => {
    const select = scanToSelect.$$('select');
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(myFiles, select.options[0].textContent.trim());
    assertEquals(selectFolderText, select.options[1].textContent.trim());
  });

  // Verifies the 'Scan To' dropdown updates when the user chooses a folder in
  // the select dialog.
  test('selectFolderDialog', () => {
    const googleDrivePath = '/this/is/a/Google/Drive';
    const googleDrive = 'Drive';
    const myDownloadsPath = '/this/is/a/test/directory/My Downloads';
    const myDownloads = 'My Downloads';

    // Simulate clicking the 'Select folder' option.
    scanningBrowserProxy.setSelectedPath(
        {baseName: myDownloads, filePath: myDownloadsPath});
    const select = scanToSelect.$$('select');
    select.selectedIndex = 1;
    select.dispatchEvent(new CustomEvent('change'));
    return flushTasks()
        .then(() => {
          assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
          assertEquals(
              myDownloads,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);

          scanningBrowserProxy.setSelectedPath(
              {baseName: googleDrive, filePath: googleDrivePath});
          select.selectedIndex = 1;
          select.dispatchEvent(new CustomEvent('change'));
          return flushTasks();
        })
        .then(() => {
          assertEquals(googleDrivePath, scanToSelect.selectedFilePath);
          assertEquals(
              googleDrive,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);
        });
  });

  // Verifys the 'Scan To' dropdown retains the previous selection when the user
  // cancels the select dialog.
  test('cancelSelectDialog', () => {
    const myDownloadsPath = '/this/is/a/test/directory/My Downloads';
    const myDownloads = 'My Downloads';

    // Simulate clicking the 'Select folder' option.
    scanningBrowserProxy.setSelectedPath(
        {baseName: myDownloads, filePath: myDownloadsPath});
    const select = scanToSelect.$$('select');
    select.selectedIndex = 1;
    select.dispatchEvent(new CustomEvent('change'));
    return flushTasks()
        .then(() => {
          assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
          assertEquals(
              myDownloads,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);

          // Simulate canceling the select dialog
          scanningBrowserProxy.setSelectedPath({baseName: '', filePath: ''});
          select.selectedIndex = 1;
          select.dispatchEvent(new CustomEvent('change'));
          return flushTasks();
        })
        .then(() => {
          assertEquals(myDownloadsPath, scanToSelect.selectedFilePath);
          assertEquals(
              myDownloads,
              select.options[select.selectedIndex].textContent.trim());
          assertEquals(0, select.selectedIndex);
        });
  });
});
