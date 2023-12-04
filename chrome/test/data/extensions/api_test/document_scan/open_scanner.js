// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

OperationResult = chrome.documentScan.OperationResult;

async function getScannerList(filter) {
  return new Promise(resolve => {
    chrome.documentScan.getScannerList(filter, resolve);
  });
}

async function openScanner(scannerId) {
  return new Promise(resolve => {
    chrome.documentScan.openScanner(scannerId, resolve);
  });
}

async function closeScanner(scannerHandle) {
  return new Promise(resolve => {
    chrome.documentScan.closeScanner(scannerHandle, resolve);
  });
}

async function getScannerId() {
  const filter = {
    local: true,
    secure: true,
  };
  let response = await getScannerList(filter);
  if (response.result != OperationResult.SUCCESS) {
    return null;
  }
  if (response.scanners.length < 1) {
    return null;
  }
  return response.scanners[0].scannerId;
}

chrome.test.runTests([
  async function openBeforeListFails() {
    let response = await openScanner("scanner");
    chrome.test.assertEq("scanner", response.scannerId);
    chrome.test.assertEq(OperationResult.INVALID, response.result);
    chrome.test.assertEq(null, response.scannerHandle);
    chrome.test.assertEq(null, response.scannerOptions);
    chrome.test.succeed();
  },

  async function closeBeforeOpenFails() {
    let response = await closeScanner("scanner");
    chrome.test.assertEq("scanner", response.scannerHandle);
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
  }
]);
