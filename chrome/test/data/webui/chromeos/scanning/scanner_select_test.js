// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/scanner_select.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ScannerArr, ScannerInfo} from 'chrome://scanning/scanning_app_types.js';
import {getScannerDisplayName, tokenToString} from 'chrome://scanning/scanning_app_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertOrderedAlphabetically, createScanner} from './scanning_app_test_utils.js';

const firstScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 1});
const firstScannerName = 'Scanner 1';

const secondScannerId =
    /** @type {!mojoBase.mojom.UnguessableToken} */ ({high: 0, low: 2});
const secondScannerName = 'Scanner 2';

suite('scannerSelectTest', function() {
  /** @type {?ScannerSelectElement} */
  let scannerSelect = null;

  setup(() => {
    scannerSelect = /** @type {!ScannerSelectElement} */ (
        document.createElement('scanner-select'));
    assertTrue(!!scannerSelect);
    scannerSelect.loaded = false;
    scannerSelect.scannerInfoMap = new Map();
    document.body.appendChild(scannerSelect);
  });

  teardown(() => {
    scannerSelect.remove();
    scannerSelect = null;
  });

  // Verify the scanner select is initialized enabled with two expected
  // scanners and the first scanner selected.
  test('initializeScannerSelect', () => {
    const select = scannerSelect.$$('select');
    assertTrue(!!select);

    const scannerArr = [
      createScanner(firstScannerId, firstScannerName),
      createScanner(secondScannerId, secondScannerName),
    ];
    scannerSelect.scanners = scannerArr;
    flush();

    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(firstScannerName, select.options[0].textContent.trim());
    assertEquals(secondScannerName, select.options[1].textContent.trim());
    assertEquals(tokenToString(firstScannerId), select.value);
  });

  // Verify the scanners are sorted alphabetically.
  test('scannersSortedAlphabetically', () => {
    const scanners = [
      createScanner(secondScannerId, secondScannerName),
      createScanner(firstScannerId, firstScannerName),
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

  // Verify the last used scanner is selected if available.
  test('selectLastUsedScanner', () => {
    const secondScannerIdString = tokenToString(secondScannerId);
    const secondScannerInfo = /** @type {!ScannerInfo} */ ({
      token: secondScannerId,
      displayName: secondScannerName,
    });
    const scanners = [
      createScanner(firstScannerId, firstScannerName),
      createScanner(secondScannerId, secondScannerName),
    ];

    scannerSelect.scannerInfoMap.set(secondScannerIdString, secondScannerInfo);
    scannerSelect.lastUsedScannerId = secondScannerIdString;
    scannerSelect.scanners = scanners;

    return waitAfterNextRender(scannerSelect).then(() => {
      assertEquals(secondScannerIdString, scannerSelect.selectedScannerId);
      assertEquals(secondScannerIdString, scannerSelect.$$('select').value);
    });
  });

  // Verify the first scanner in the dropdown is selected when the last used
  // scanner is not set.
  test('selectFirtScanner', () => {
    const scanners = [
      createScanner(secondScannerId, secondScannerName),
      createScanner(firstScannerId, firstScannerName),
    ];

    scannerSelect.lastUsedScannerId = '';
    scannerSelect.scanners = scanners;

    const firstScannerIdString = tokenToString(firstScannerId);
    return waitAfterNextRender(scannerSelect).then(() => {
      assertEquals(firstScannerIdString, scannerSelect.selectedScannerId);
      assertEquals(firstScannerIdString, scannerSelect.$$('select').value);
    });
  });
});
