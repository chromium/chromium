// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([() => {
  chrome.printing.getPrinterInfo('id', response => {
    chrome.test.assertNe(undefined, response);
    chrome.test.assertNe(undefined, response.capabilities);
    chrome.test.assertNe(undefined, response.capabilities.printer);
    let color = response.capabilities.printer.color;
    chrome.test.assertNe(undefined, color);
    chrome.test.assertEq(1, color.option.length);
    chrome.test.assertNe(undefined, color.option[0]);
    chrome.test.assertEq('STANDARD_MONOCHROME', color.option[0].type);

    chrome.test.assertNe(undefined, response.status);
    chrome.test.assertEq(
        chrome.printing.PrinterStatus.UNREACHABLE, response.status);

    chrome.test.succeed();
  });
}]);
