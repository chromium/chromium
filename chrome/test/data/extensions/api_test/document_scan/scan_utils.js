// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

OperationResult = chrome.documentScan.OperationResult;
OptionType = chrome.documentScan.OptionType;

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

async function getOptionGroups(scannerHandle) {
 return new Promise(resolve => {
    chrome.documentScan.getOptionGroups(scannerHandle, resolve);
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

async function startScan(scannerHandle, maxReadSize) {
  return new Promise(resolve => {
    let options = {
      format: 'format',
    };
    if (maxReadSize != undefined) {
      options['maxReadSize'] = maxReadSize;
    }
    chrome.documentScan.startScan(scannerHandle, options, resolve);
  });
}

async function cancelScan(jobHandle) {
  return new Promise(resolve => {
    chrome.documentScan.cancelScan(jobHandle, resolve);
  });
}

async function readScanData(jobHandle) {
  return new Promise(resolve => {
    chrome.documentScan.readScanData(jobHandle, resolve);
  });
}

async function setOptions(scannerHandle, options) {
  return new Promise(resolve => {
    chrome.documentScan.setOptions(scannerHandle, options, resolve);
  });
}
