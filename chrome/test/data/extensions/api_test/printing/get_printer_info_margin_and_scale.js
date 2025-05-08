// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([() => {
  chrome.printing.getPrinterInfo('id', response => {
    chrome.test.assertNe(undefined, response);
    chrome.test.assertNe(undefined, response.capabilities);
    chrome.test.assertNe(undefined, response.capabilities.printer);

    let fit_to_page = response.capabilities.printer.fit_to_page;
    chrome.test.assertNe(undefined, fit_to_page);
    chrome.test.assertEq(3, fit_to_page.option.length);
    chrome.test.assertNe(undefined, fit_to_page.option[0]);
    chrome.test.assertEq(undefined, fit_to_page.option[0].is_default);
    chrome.test.assertEq('FIT', fit_to_page.option[0].type);

    chrome.test.assertNe(undefined, fit_to_page.option[1]);
    chrome.test.assertEq(undefined, fit_to_page.option[1].is_default);
    chrome.test.assertEq('AUTO', fit_to_page.option[1].type);

    chrome.test.assertNe(undefined, fit_to_page.option[2]);
    chrome.test.assertNe(undefined, fit_to_page.option[2].is_default);
    chrome.test.assertEq('AUTO', fit_to_page.option[2].type);
    chrome.test.assertEq(true, fit_to_page.option[2].is_default);

    let margin = response.capabilities.printer.margins;
    chrome.test.assertNe(undefined, margin);
    chrome.test.assertEq(2, margin.option.length);
    chrome.test.assertNe(undefined, margin.option[0]);
    chrome.test.assertNe(undefined, margin.option[0].is_default);
    chrome.test.assertEq(5008, margin.option[0].bottom_microns);
    chrome.test.assertEq(3050, margin.option[0].left_microns);
    chrome.test.assertEq(3002, margin.option[0].right_microns);
    chrome.test.assertEq(1003, margin.option[0].top_microns);

    chrome.test.assertNe(undefined, margin.option[1]);
    chrome.test.assertEq(undefined, margin.option[1].is_default);
    chrome.test.assertEq(0, margin.option[1].bottom_microns);
    chrome.test.assertEq(0, margin.option[1].left_microns);
    chrome.test.assertEq(0, margin.option[1].right_microns);
    chrome.test.assertEq(0, margin.option[1].top_microns);

    chrome.test.succeed();
  });
}]);
