// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jschettler): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://scanning/scanning_app.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {setScanServiceForTesting} from 'chrome://scanning/mojo_interface_provider.js';
import {ScannerArr} from 'chrome://scanning/scanning_app_types.js';
import {getSourceTypeString, tokenToString} from 'chrome://scanning/scanning_app_util.js';

const ColorMode = {
  BLACK_AND_WHITE: chromeos.scanning.mojom.ColorMode.kBlackAndWhite,
  GRAYSCALE: chromeos.scanning.mojom.ColorMode.kGrayscale,
  COLOR: chromeos.scanning.mojom.ColorMode.kColor,
};

const SourceType = {
  FLATBED: chromeos.scanning.mojom.SourceType.kFlatbed,
  ADF_SIMPLEX: chromeos.scanning.mojom.SourceType.kAdfSimplex,
  ADF_DUPLEX: chromeos.scanning.mojom.SourceType.kAdfDuplex,
};

/**
 * @param {!mojoBase.mojom.UnguessableToken} id
 * @param {!string} displayName
 * @return {!chromeos.scanning.mojom.Scanner}
 */
function createScanner(id, displayName) {
  let scanner = {
    'id': id,
    'displayName': strToMojoString16(displayName),
  };
  return scanner;
}

/**
 * @param {number} type
 * @param {!string} name
 * @return {!chromeos.scanning.mojom.ScanSource}
 */
function createSource(type, name) {
  let source = {
    'type': type,
    'name': name,
  };
  return source;
}

/**
 * Converts a JS string to a mojo_base::mojom::String16 object.
 * @param {!string} str
 * @return {!object}
 */
function strToMojoString16(str) {
  let arr = [];
  for (var i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }

  return {data: arr};
}

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

  test('Scan', () => {
    const firstScannerId = {high: 0, low: 1};
    const firstScannerName = 'Scanner 1';
    const secondScannerId = {high: 0, low: 2};
    const secondScannerName = 'Scanner 2';
    const expectedScanners = [
      createScanner(firstScannerId, firstScannerName),
      createScanner(secondScannerId, secondScannerName)
    ];

    const firstCapabilities = {
      sources: [
        {type: SourceType.FLATBED, name: 'platen'},
        {type: SourceType.ADF_DUPLEX, name: 'adf duplex'}
      ],
      colorModes: [ColorMode.BLACK_AND_WHITE, ColorMode.COLOR],
      resolutions: [75, 100, 300]
    };
    const secondCapabilities = {
      sources: [{type: SourceType.ADF_SIMPLEX, name: 'adf simplex'}],
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

          // Before the scan button is clicked, the settings and scan button
          // should be enabled, and there should be no scan status.
          const scannerSelect = scanningApp.$$('#scannerSelect').$$('select');
          assertFalse(scannerSelect.disabled);
          const sourceSelect = scanningApp.$$('#sourceSelect').$$('select');
          assertFalse(sourceSelect.disabled);
          const scanButton = scanningApp.$$('#scanButton');
          assertFalse(scanButton.disabled);
          const statusText = scanningApp.$$('#statusText');
          assertEquals('', statusText.textContent.trim());
          scanButton.click();

          // After the scan button is clicked, the settings and scan button
          // should be disabled, and the scan status should indicate that
          // scanning is in progress.
          assertTrue(scannerSelect.disabled);
          assertTrue(sourceSelect.disabled);
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
          assertFalse(scanningApp.$$('#scanButton').disabled);
          assertEquals(
              'Scan complete! File(s) saved to My files.',
              scanningApp.$$('#statusText').textContent.trim());
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
      createScanner(firstScannerId, firstScannerName),
      createScanner(secondScannerId, secondScannerName)
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

    let scannerArr = [createScanner({high: 0, low: 1}, 'Scanner 1')];
    scannerSelect.scanners = scannerArr;
    scannerSelect.loaded = true;
    flush();

    // Verify the dropdown is disabled when there's only one option.
    assertEquals(1, select.length);
    assertTrue(select.disabled);

    scannerArr =
        scannerArr.concat([createScanner({high: 0, low: 2}, 'Scanner 2')]);
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

    const firstSource = createSource(SourceType.FLATBED, 'platen');
    const secondSource = createSource(SourceType.ADF_SIMPLEX, 'adf simplex');
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

    let sourceArr = [createSource(SourceType.FLATBED, 'flatbed')];
    sourceSelect.sources = sourceArr;
    flush();

    // Verify the dropdown is disabled when there's only one option.
    assertEquals(1, select.length);
    assertTrue(select.disabled);

    sourceArr =
        sourceArr.concat([createSource(SourceType.ADF_DUPLEX, 'adf duplex')]);
    sourceSelect.sources = sourceArr;
    flush();

    // Verify the dropdown is enabled when there's more than one option.
    assertEquals(2, select.length);
    assertFalse(select.disabled);
  });
});
