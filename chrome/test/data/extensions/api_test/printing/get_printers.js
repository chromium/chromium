// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetPrinters(expectedName) {
  chrome.test.runTests([() => {
    chrome.printing.getPrinters(printers => {
      chrome.test.assertEq(1, printers.length);
      chrome.test.assertEq(expectedName, printers[0].name);
      chrome.test.succeed();
    });
  }]);
}

testGetPrinters("name");
