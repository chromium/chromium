// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jschettler): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://scanning/scanning_app.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {setScanServiceForTesting} from 'chrome://scanning/mojo_interface_provider.js';
import {ScannerArr} from 'chrome://scanning/scanning_app_types.js';
import {tokenToString} from 'chrome://scanning/scanning_app_util.js';

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

    this.resetForTest();
  }

  resetForTest() {
    this.scanners_ = [];
    this.resolverMap_.set('getScanners', new PromiseResolver());
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

  // scanService methods:

  /** @return {!Promise<{scanners: !ScannerArr}>} */
  getScanners() {
    return new Promise(resolve => {
      this.methodCalled('getScanners');
      resolve({scanners: this.scanners_ || []});
    });
  }
}

suite('ScanningAppTest', () => {
  /** @type {?ScanningAppElement} */
  let page = null;

  /** @type {?chromeos.scanning.mojom.ScanServiceRemote} */
  let fakeScanService_;

  suiteSetup(() => {
    fakeScanService_ = new FakeScanService();
    setScanServiceForTesting(fakeScanService_);
  });

  setup(function() {
    PolymerTest.clearBody();
    page = document.createElement('scanning-app');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
    page = null;
  });

  test('MainPageLoaded', () => {});
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
