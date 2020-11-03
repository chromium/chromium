// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/scan_preview.js';
import 'chrome://scanning/scanning_app.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ScannerArr} from 'chrome://scanning/scanning_app_types.js';
import {tokenToString} from 'chrome://scanning/scanning_app_util.js';

import * as utils from './scanning_app_test_utils.js';

export function scannerSelectTest() {
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
}
