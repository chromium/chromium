// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests should be run with the get scanner list permission allowed and
// the start scan permission denied (and the extension should not be trusted).
chrome.test.runTests([
  async function startScanDenied() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const options = {
      format: 'format',
    };
    chrome.documentScan.startScan(scannerHandle, options, response => {
      chrome.test.assertEq(chrome.documentScan.OperationResult.ACCESS_DENIED,
                           response.result);
      chrome.test.assertEq(scannerHandle, response.scannerHandle);
      chrome.test.assertEq(null, response.job);
      chrome.test.succeed();
    });
  }
]);
