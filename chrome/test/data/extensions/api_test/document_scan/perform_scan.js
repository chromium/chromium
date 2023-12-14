// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests should be run with get scanner list permission allowed and start
// scan permission allowed (or the extension should be trusted).
chrome.test.runTests([
  async function getScannerListTwoCalls() {
    const filter = {
      local: true,
      secure: true,
    };
    var savedId;
    chrome.documentScan.getScannerList(filter, response => {
      chrome.test.assertEq(OperationResult.SUCCESS, response.result);
      chrome.test.assertEq(1, response.scanners.length);
      chrome.test.assertNe('', response.scanners[0].scannerId);
      savedId = response.scanners[0].scannerId;
      chrome.test.assertEq('GoogleTest', response.scanners[0].manufacturer);
      chrome.test.assertEq('Scanner', response.scanners[0].model);

      // Second call should get the same scanner with a different id.
      chrome.documentScan.getScannerList(filter, response => {
        chrome.test.assertEq(OperationResult.SUCCESS, response.result);
        chrome.test.assertEq(1, response.scanners.length);
        chrome.test.assertNe('', response.scanners[0].scannerId);
        chrome.test.assertNe(savedId, response.scanners[0].scannerId);
        chrome.test.assertEq('GoogleTest', response.scanners[0].manufacturer);
        chrome.test.assertEq('Scanner', response.scanners[0].model);
        chrome.test.succeed();
      });
    });
  },

  async function openBeforeListFails() {
    let response = await openScanner('scanner');
    chrome.test.assertEq('scanner', response.scannerId);
    chrome.test.assertEq(OperationResult.INVALID, response.result);
    chrome.test.assertEq(null, response.scannerHandle);
    chrome.test.assertEq(null, response.scannerOptions);
    chrome.test.succeed();
  },

  async function closeBeforeOpenFails() {
    let response = await closeScanner('scanner');
    chrome.test.assertEq('scanner', response.scannerHandle);
    chrome.test.assertEq(OperationResult.INVALID, response.result);
    chrome.test.succeed();
  },

  async function closeOpenHandleSucceeds() {
    let scannerId = await getScannerId();
    chrome.test.assertNe(null, scannerId);

    let openResponse = await openScanner(scannerId);
    chrome.test.assertEq(scannerId, openResponse.scannerId);
    chrome.test.assertEq(OperationResult.SUCCESS, openResponse.result);
    chrome.test.assertNe(null, openResponse.scannerHandle);
    const scannerHandle = openResponse.scannerHandle;

    let closeResponse = await closeScanner(scannerHandle);
    chrome.test.assertEq(scannerHandle, closeResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.SUCCESS, closeResponse.result);

    // Closing the same handle a second time fails.
    closeResponse = await closeScanner(scannerHandle);
    chrome.test.assertEq(scannerHandle, closeResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.INVALID, closeResponse.result);

    chrome.test.succeed();
  },

  async function reopenHandleSucceeds() {
    let scannerId = await getScannerId();
    chrome.test.assertNe(null, scannerId);

    let openResponse = await openScanner(scannerId);
    chrome.test.assertEq(scannerId, openResponse.scannerId);
    chrome.test.assertEq(OperationResult.SUCCESS, openResponse.result);
    chrome.test.assertNe(null, openResponse.scannerHandle);

    // Reopening the same scannerId succeeds.  Check for a non-empty handle, but
    // don't check if it's different from the previous handle because the
    // backend doesn't guarantee to send a different value each time.
    openResponse = await openScanner(scannerId);
    chrome.test.assertEq(scannerId, openResponse.scannerId);
    chrome.test.assertEq(OperationResult.SUCCESS, openResponse.result);
    chrome.test.assertNe(null, openResponse.scannerHandle);

    // Get a new scannerId pointing to the same scanner.
    scannerId = await getScannerId();
    chrome.test.assertNe(null, scannerId);

    // Reopening the same scanner via a new scannerId succeeds.
    openResponse = await openScanner(scannerId);
    chrome.test.assertEq(scannerId, openResponse.scannerId);
    chrome.test.assertEq(OperationResult.SUCCESS, openResponse.result);
    chrome.test.assertNe(null, openResponse.scannerHandle);

    chrome.test.succeed();
  },

  async function startAndCancelScan() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const startResponse = await startScan(scannerHandle);
    const jobHandle = startResponse.job;
    chrome.test.assertEq(scannerHandle, startResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.SUCCESS, startResponse.result);
    chrome.test.assertNe(null, jobHandle);

    const cancelResponse = await cancelScan(jobHandle);
    chrome.test.assertEq(jobHandle, cancelResponse.job);
    chrome.test.assertEq(OperationResult.SUCCESS, cancelResponse.result);
    chrome.test.succeed();
  },

  async function startScanInvalidHandleFails() {
    const startResponse = await startScan('invalid-handle');
    chrome.test.assertEq('invalid-handle', startResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.INVALID, startResponse.result);
    chrome.test.assertEq(null, startResponse.job);
    chrome.test.succeed();
  },

  async function cancelScanInvalidHandleFails() {
    const cancelResponse = await cancelScan('invalid-handle');
    chrome.test.assertEq('invalid-handle', cancelResponse.job);
    chrome.test.assertEq(OperationResult.INVALID, cancelResponse.result);
    chrome.test.succeed();
  },

  async function getListMaintainsJob() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const startResponse = await startScan(scannerHandle);
    const jobHandle = startResponse.job;
    chrome.test.assertEq(scannerHandle, startResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.SUCCESS, startResponse.result);
    chrome.test.assertNe(null, jobHandle);

    // If a user calls getScannerList, an open job handle will remain valid and
    // should be cancelable.
    const filter = {
      local: true,
      secure: true,
    };
    let getListResponse = await getScannerList(filter);
    chrome.test.assertEq(OperationResult.SUCCESS, getListResponse.result);

    const cancelResponse = await cancelScan(jobHandle);
    chrome.test.assertEq(jobHandle, cancelResponse.job);
    chrome.test.assertEq(OperationResult.SUCCESS, cancelResponse.result);
    chrome.test.succeed();
  }

]);
