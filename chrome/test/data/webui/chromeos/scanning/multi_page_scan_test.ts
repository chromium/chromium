// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/multi_page_scan.js';

import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {MultiPageScanElement} from 'chrome://scanning/multi_page_scan.js';
import {AppState} from 'chrome://scanning/scanning_app_types.js';
import {ScanningBrowserProxyImpl} from 'chrome://scanning/scanning_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestScanningBrowserProxy} from './test_scanning_browser_proxy.js';

suite('multiPageScanTest', function() {
  let multiPageScan: MultiPageScanElement|null = null;

  let scanningBrowserProxy: TestScanningBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    scanningBrowserProxy = new TestScanningBrowserProxy();
    ScanningBrowserProxyImpl.setInstance(scanningBrowserProxy);

    multiPageScan = document.createElement('multi-page-scan');
    assertTrue(!!multiPageScan);
    document.body.appendChild(multiPageScan);
  });

  teardown(() => {
    multiPageScan?.remove();
    multiPageScan = null;
    scanningBrowserProxy.reset();
  });

  // Verify the Scan button correctly reflects the next page number to scan.
  test('scanButtonPageNumUpdates', async () => {
    assert(multiPageScan);
    multiPageScan.pageNumber = 1;
    await flushTasks();
    const scanButton =
        strictQuery('#scanButton', multiPageScan.shadowRoot, CrButtonElement);
    assertEquals('Scan page 2', scanButton.textContent!.trim());
    multiPageScan.pageNumber = 2;
    await flushTasks();
    assertEquals('Scan page 3', scanButton.textContent!.trim());
  });

  // Verify clicking the Scan button fires the 'scan-next-page' event.
  test('scanButtonFiresEvent', async () => {
    assert(multiPageScan);
    let scanNextPageEventFired = false;
    multiPageScan.addEventListener('scan-next-page', function() {
      scanNextPageEventFired = true;
    });
    const scanNextPageEvent = eventToPromise('scan-next-page', multiPageScan);
    strictQuery('#scanButton', multiPageScan.shadowRoot, HTMLElement).click();
    await scanNextPageEvent;
    assertTrue(scanNextPageEventFired);
  });

  // Verify clicking the Save button fires the 'complete-multi-page-scan' event.
  test('saveButtonFiresEvent', async () => {
    assert(multiPageScan);
    let completeMultiPageScanEventFired = false;
    multiPageScan.addEventListener('complete-multi-page-scan', function() {
      completeMultiPageScanEventFired = true;
    });
    const scanCompleteEvent =
        eventToPromise('complete-multi-page-scan', multiPageScan);
    strictQuery('#saveButton', multiPageScan.shadowRoot, HTMLElement).click();
    await scanCompleteEvent;
    assertTrue(completeMultiPageScanEventFired);
  });

  // Verify the cancel button and progress text show while scanning.
  test('cancelButtonShowsWhileScanning', () => {
    assert(multiPageScan);
    const scanButton =
        strictQuery('#scanButton', multiPageScan.shadowRoot, HTMLElement);
    const cancelButton =
        strictQuery('#cancelButton', multiPageScan.shadowRoot, HTMLElement);
    const pageNumber = 4;

    // Scan button should be visible.
    multiPageScan.appState = AppState.MULTI_PAGE_NEXT_ACTION;
    assertTrue(isVisible(scanButton));
    assertFalse(isVisible(cancelButton));
    assertEquals(
        loadTimeData.getString('multiPageScanInstructionsText'),
        strictQuery('#multiPageScanText', multiPageScan.shadowRoot, HTMLElement)
            .innerText.trim());

    // Cancel button should be visible while scanning.
    multiPageScan.pageNumber = pageNumber;
    multiPageScan.appState = AppState.MULTI_PAGE_SCANNING;
    assertFalse(isVisible(scanButton));
    assertTrue(isVisible(cancelButton));
    assertEquals(
        loadTimeData.getStringF('multiPageScanProgressText', pageNumber),
        strictQuery('#multiPageScanText', multiPageScan.shadowRoot, HTMLElement)
            .innerText.trim());
  });

  // Verify the cancel button and canceling text are shown while canceling a
  // scan.
  test('cancelButtonShowsWhileCanceling', () => {
    assert(multiPageScan);
    const cancelButton = strictQuery(
        '#cancelButton', multiPageScan.shadowRoot, CrButtonElement)!;

    // Cancel button should be visible and disabled.
    multiPageScan.appState = AppState.MULTI_PAGE_CANCELING;
    assertTrue(isVisible(cancelButton));
    assertTrue(cancelButton.disabled);
    assertEquals(
        loadTimeData.getString('multiPageCancelingScanningText'),
        strictQuery('#multiPageScanText', multiPageScan.shadowRoot, HTMLElement)
            .innerText.trim());
  });

  // Verify clicking the cancel button fires the 'cancel-click' event.
  test('clickCancelButton', async () => {
    assert(multiPageScan);
    let clickCancelEventFired = false;
    multiPageScan.addEventListener('cancel-click', function() {
      clickCancelEventFired = true;
    });
    const cancelEvent = eventToPromise('cancel-click', multiPageScan);
    strictQuery('#cancelButton', multiPageScan.shadowRoot, HTMLElement).click();
    await cancelEvent;
    assertTrue(clickCancelEventFired);
  });
});
