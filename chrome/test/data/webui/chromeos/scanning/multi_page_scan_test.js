// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/multi_page_scan.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {AppState} from 'chrome://scanning/scanning_app_types.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

suite('multiPageScanTest', function() {
  /** @type {?MultiPageScanElement} */
  let multiPageScan = null;

  /** @type {?TestScanningBrowserProxy} */
  let scanningBrowserProxy = null;

  setup(() => {
    scanningBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.instance_ = scanningBrowserProxy;

    multiPageScan = /** @type {!MultiPageScanElement} */ (
        document.createElement('multi-page-scan'));
    assertTrue(!!multiPageScan);
    document.body.appendChild(multiPageScan);
  });

  teardown(() => {
    if (multiPageScan) {
      multiPageScan.remove();
    }
    multiPageScan = null;
    scanningBrowserProxy.reset();
  });

  // Verify the Scan button correctly reflects the next page number to scan.
  test('scanButtonPageNumUpdates', () => {
    multiPageScan.pageNumber = 1;
    return flushTasks()
        .then(() => {
          assertEquals(
              'Scan page 2',
              multiPageScan.$$('#scanButton').textContent.trim());
          multiPageScan.pageNumber = 2;
          return flushTasks();
        })
        .then(() => {
          assertEquals(
              'Scan page 3',
              multiPageScan.$$('#scanButton').textContent.trim());
        });
  });

  // Verify clicking the Scan button fires the 'scan-next-page' event.
  test('scanButtonFiresEvent', () => {
    let scanNextPageEventFired = false;
    multiPageScan.addEventListener('scan-next-page', function() {
      scanNextPageEventFired = true;
    });

    multiPageScan.$$('#scanButton').click();
    assertTrue(scanNextPageEventFired);
  });

  // Verify clicking the Save button fires the 'complete-multi-page-scan' event.
  test('saveButtonFiresEvent', () => {
    let completeMultiPageScanEventFired = false;
    multiPageScan.addEventListener('complete-multi-page-scan', function() {
      completeMultiPageScanEventFired = true;
    });

    multiPageScan.$$('#saveButton').click();
    assertTrue(completeMultiPageScanEventFired);
  });

  // Verify the cancel button and progress text show while scanning.
  test('cancelButtonShowsWhileScanning', () => {
    const scanButton =
        /** @type {!HTMLElement} */ (multiPageScan.$$('#scanButton'));
    const cancelButton =
        /** @type {!HTMLElement} */ (multiPageScan.$$('#cancelButton'));
    const pageNumber = 4;

    // Scan button should be visible.
    multiPageScan.appState = AppState.MULTI_PAGE_NEXT_ACTION;
    assertTrue(isVisible(scanButton));
    assertFalse(isVisible(cancelButton));
    assertEquals(
        loadTimeData.getString('multiPageScanInstructionsText'),
        multiPageScan.$$('#multiPageScanText').innerText.trim());

    // Cancel button should be visible while scanning.
    multiPageScan.pageNumber = pageNumber;
    multiPageScan.appState = AppState.MULTI_PAGE_SCANNING;
    assertFalse(isVisible(scanButton));
    assertTrue(isVisible(cancelButton));
    assertEquals(
        loadTimeData.getStringF('multiPageScanProgressText', pageNumber),
        multiPageScan.$$('#multiPageScanText').innerText.trim());
  });

  // Verify the cancel button and canceling text are shown while canceling a
  // scan.
  test('cancelButtonShowsWhileCanceling', () => {
    const cancelButton =
        /** @type {!HTMLElement} */ (multiPageScan.$$('#cancelButton'));

    // Cancel button should be visible and disabled.
    multiPageScan.appState = AppState.MULTI_PAGE_CANCELING;
    assertTrue(isVisible(cancelButton));
    assertTrue(cancelButton.disabled);
    assertEquals(
        loadTimeData.getString('multiPageCancelingScanningText'),
        multiPageScan.$$('#multiPageScanText').innerText.trim());
  });

  // Verify clicking the cancel button fires the 'cancel-click' event.
  test('clickCancelButton', () => {
    let clickCancelEventFired = false;
    multiPageScan.addEventListener('cancel-click', function() {
      clickCancelEventFired = true;
    });

    multiPageScan.$$('#cancelButton').click();
    assertTrue(clickCancelEventFired);
  });
});
