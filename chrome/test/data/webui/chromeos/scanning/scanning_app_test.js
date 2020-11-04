// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scan_preview.js';
import 'chrome://scanning/scanning_app.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {setScanServiceForTesting} from 'chrome://scanning/mojo_interface_provider.js';
import {ScannerArr} from 'chrome://scanning/scanning_app_types.js';
import {tokenToString} from 'chrome://scanning/scanning_app_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as utils from './scanning_app_test_utils.js';

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

/** @implements {chromeos.scanning.mojom.ScanServiceInterface} */
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

export function scanningAppTest() {
  /** @type {?ScanningAppElement} */
  let scanningApp = null;

  /** @type {?FakeScanService} */
  let fakeScanService_ = null;

  suiteSetup(() => {
    fakeScanService_ = new FakeScanService();
    setScanServiceForTesting(fakeScanService_);
  });

  setup(function() {
    document.body.innerHTML = '';
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
    scanningApp = /** @type {!ScanningAppElement} */ (
        document.createElement('scanning-app'));
    document.body.appendChild(scanningApp);
    assert(!!scanningApp);
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
   * Returns whether the "More settings" section is expanded or not.
   * @return {boolean}
   */
  function isSettingsOpen() {
    return scanningApp.$.collapse.opened;
  }

  test('Scan', () => {
    const firstScannerId =
        /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 1});
    const firstScannerName = 'Scanner 1';

    const secondScannerId =
        /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 2});
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
}
