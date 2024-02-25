// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/scanner_select.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ScannerSelectElement} from 'chrome://scanning/scanner_select.js';
import {Scanner} from 'chrome://scanning/scanning.mojom-webui.js';
import {ScannerInfo} from 'chrome://scanning/scanning_app_types.js';
import {getScannerDisplayName, tokenToString} from 'chrome://scanning/scanning_app_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertOrderedAlphabetically, createScanner} from './scanning_app_test_utils.js';

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

suite('scannerSelectTest', function() {
  let scannerSelect: ScannerSelectElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    scannerSelect = document.createElement('scanner-select');
    assertTrue(!!scannerSelect);
    scannerSelect.scannerInfoMap = new Map<string, ScannerInfo>();
    document.body.appendChild(scannerSelect);
  });

  teardown(() => {
    scannerSelect?.remove();
    scannerSelect = null;
  });


  function getSelect(): HTMLSelectElement {
    assert(scannerSelect);
    const select =
        strictQuery('select', scannerSelect.shadowRoot, HTMLSelectElement);
    assert(select);
    return select;
  }

  function getOption(index: number): HTMLOptionElement {
    const options = Array.from(getSelect().querySelectorAll('option'));
    assert(index < options.length);
    return options[index]!;
  }

  // Verify the scanner select is initialized enabled with two expected
  // scanners and the first scanner selected.
  test('initializeScannerSelect', () => {
    assert(scannerSelect);
    const select = getSelect();
    assertTrue(!!select);

    const scannerArr = [
      createScanner(firstScannerId, firstScannerName),
      createScanner(secondScannerId, secondScannerName),
    ];
    scannerSelect.scanners = scannerArr;
    flush();

    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(firstScannerName, getOption(0).textContent!.trim());
    assertEquals(secondScannerName, getOption(1).textContent!.trim());
    assertEquals(tokenToString(firstScannerId), select.value);
  });

  // Verify the scanners are sorted alphabetically.
  test('scannersSortedAlphabetically', () => {
    assert(scannerSelect);
    const scanners = [
      createScanner(secondScannerId, secondScannerName),
      createScanner(firstScannerId, firstScannerName),
    ];
    scannerSelect.scanners = scanners;
    flush();

    // Verify the scanners are sorted alphabetically and that the first scanner
    // in the sorted array is selected.
    assertOrderedAlphabetically(
        scannerSelect.scanners,
        (scanner: Scanner) => getScannerDisplayName(scanner));
    assertEquals(
        tokenToString(firstScannerId), scannerSelect.selectedScannerId);
  });

  // Verify the last used scanner is selected if available.
  test('selectLastUsedScanner', async () => {
    assert(scannerSelect);
    assert(scannerSelect);
    const secondScannerIdString = tokenToString(secondScannerId);
    const secondScannerInfo: ScannerInfo = {
      token: secondScannerId,
      displayName: secondScannerName,
    };
    const scanners = [
      createScanner(firstScannerId, firstScannerName),
      createScanner(secondScannerId, secondScannerName),
    ];

    scannerSelect.scannerInfoMap.set(secondScannerIdString, secondScannerInfo);
    scannerSelect.lastUsedScannerId = secondScannerIdString;
    scannerSelect.scanners = scanners;

    await waitAfterNextRender(scannerSelect);
    assertEquals(secondScannerIdString, scannerSelect.selectedScannerId);
    assertEquals(secondScannerIdString, getSelect().value);
  });

  // Verify the first scanner in the dropdown is selected when the last used
  // scanner is not set.
  test('selectFirstScanner', async () => {
    assert(scannerSelect);
    const scanners = [
      createScanner(secondScannerId, secondScannerName),
      createScanner(firstScannerId, firstScannerName),
    ];

    scannerSelect.lastUsedScannerId = '';
    scannerSelect.scanners = scanners;

    const firstScannerIdString = tokenToString(firstScannerId);
    await waitAfterNextRender(scannerSelect);
    assertEquals(firstScannerIdString, scannerSelect.selectedScannerId);
    assertEquals(firstScannerIdString, getSelect().value);
  });
});
