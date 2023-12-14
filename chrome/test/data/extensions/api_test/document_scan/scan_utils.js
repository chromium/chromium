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

async function getScannerHandle() {
  const scannerId = await getScannerId();
  if (scannerId == null) {
    return null;
  }

  let openResponse = await openScanner(scannerId);
  return openResponse.scannerHandle;
}

async function startScan(scannerHandle) {
  return new Promise(resolve => {
    const options = {
      format: 'format',
    };
    chrome.documentScan.startScan(scannerHandle, options, resolve);
  });
}

async function cancelScan(jobHandle) {
  return new Promise(resolve => {
    chrome.documentScan.cancelScan(jobHandle, resolve);
  });
}
