// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([() => {
  const filter = {
    local: true,
    secure: true,
  };
  chrome.documentScan.getScannerList(filter, response => {
    chrome.test.assertEq(chrome.documentScan.OperationResult.ACCESS_DENIED,
                         response.result);
    chrome.test.assertEq(0, response.scanners.length);
    chrome.test.succeed();
  });
}]);
