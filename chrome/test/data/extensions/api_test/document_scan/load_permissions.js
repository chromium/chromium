// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function apiFunctionExists() {
    chrome.test.assertTrue(!!chrome.documentScan);
    chrome.test.assertTrue(!!chrome.documentScan.scan);
    chrome.test.assertTrue(!!chrome.documentScan.getScannerList);
    chrome.test.assertTrue(!!chrome.documentScan.openScanner);
    chrome.test.assertTrue(!!chrome.documentScan.getOptionGroups);
    chrome.test.assertTrue(!!chrome.documentScan.closeScanner);
    chrome.test.assertTrue(!!chrome.documentScan.setOptions);
    chrome.test.assertTrue(!!chrome.documentScan.startScan);
    chrome.test.assertTrue(!!chrome.documentScan.cancelScan);
    chrome.test.assertTrue(!!chrome.documentScan.readScanData);
    chrome.test.succeed();
  },
]);
