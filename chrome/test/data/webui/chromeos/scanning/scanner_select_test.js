// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scanner_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ScannerArr} from 'chrome://scanning/scanning_app_types.js';
import {getScannerDisplayName, tokenToString} from 'chrome://scanning/scanning_app_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {assertOrderedAlphabetically, createScanner} from './scanning_app_test_utils.js';

const firstScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 1});
const firstScannerName = 'Scanner 1';

const secondScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 2});
const secondScannerName = 'Scanner 2';

export function scannerSelectTest() {
  /** @type {?ScannerSelectElement} */
  let scannerSelect = null;

  setup(() => {
    scannerSelect = /** @type {!ScannerSelectElement} */ (
        document.createElement('scanner-select'));
    assertTrue(!!scannerSelect);
    scannerSelect.loaded = false;
    document.body.appendChild(scannerSelect);
  });

  teardown(() => {
    scannerSelect.remove();
    scannerSelect = null;
  });

  test('initializeScannerSelect', () => {
    const select = scannerSelect.$$('select');
    assertTrue(!!select);

    const scannerArr = [
      createScanner(firstScannerId, firstScannerName),
      createScanner(secondScannerId, secondScannerName)
    ];
    scannerSelect.scanners = scannerArr;
    flush();

    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(firstScannerName, select.options[0].textContent.trim());
    assertEquals(secondScannerName, select.options[1].textContent.trim());
    assertEquals(tokenToString(firstScannerId), select.value);
  });

  test('scannersSortedAlphabetically', () => {
    const scanners = [
      createScanner(secondScannerId, secondScannerName),
      createScanner(firstScannerId, firstScannerName)
    ];
    scannerSelect.scanners = scanners;
    flush();

    // Verify the scanners are sorted alphabetically and that the first scanner
    // in the sorted array is selected.
    assertOrderedAlphabetically(
        scannerSelect.scanners, (scanner) => getScannerDisplayName(scanner));
    assertEquals(
        tokenToString(firstScannerId), scannerSelect.selectedScannerId);
  });
}
