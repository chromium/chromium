// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([() => {
  const filter = {
    local: true,
    secure: true,
  };
  var savedId;
  chrome.documentScan.getScannerList(filter, response => {
    chrome.test.assertEq(chrome.documentScan.OperationResult.SUCCESS,
                         response.result);
    chrome.test.assertEq(1, response.scanners.length);
    chrome.test.assertNe("", response.scanners[0].scannerId);
    savedId = response.scanners[0].scannerId;
    chrome.test.assertEq("GoogleTest", response.scanners[0].manufacturer);
    chrome.test.assertEq("Scanner", response.scanners[0].model);

    // Second call should get the same scanner with a different id.
    chrome.documentScan.getScannerList(filter, response => {
      chrome.test.assertEq(chrome.documentScan.OperationResult.SUCCESS,
                           response.result);
      chrome.test.assertEq(1, response.scanners.length);
      chrome.test.assertNe("", response.scanners[0].scannerId);
      chrome.test.assertNe(savedId, response.scanners[0].scannerId);
      chrome.test.assertEq("GoogleTest", response.scanners[0].manufacturer);
      chrome.test.assertEq("Scanner", response.scanners[0].model);
      chrome.test.succeed();
    });
  });
}]);
