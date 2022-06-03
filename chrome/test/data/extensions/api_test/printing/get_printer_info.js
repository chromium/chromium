// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([() => {
  chrome.printing.getPrinterInfo('id', response => {
    chrome.test.assertTrue(response != undefined);
    chrome.test.assertTrue(response.capabilities != undefined);
    chrome.test.assertTrue(response.capabilities.printer != undefined);
    let color = response.capabilities.printer.color;
    chrome.test.assertTrue(color != undefined);
    chrome.test.assertEq(1, color.option.length);
    chrome.test.assertTrue(color.option[0] != undefined);
    chrome.test.assertEq('STANDARD_MONOCHROME', color.option[0].type);

    chrome.test.assertTrue(response.status != undefined);
    chrome.test.assertEq(
        chrome.printing.PrinterStatus.UNREACHABLE, response.status);

    chrome.test.succeed();
  });
}]);
