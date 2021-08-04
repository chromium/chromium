// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/multi_page_scan.js';

import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

export function multiPageScanTest() {
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
}
