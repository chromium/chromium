// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrintPreviewPrinterSetupInfoCrosElement} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

const printer_setup_info_cros_test = {
  suiteName: 'PrinterSetupInfoCrosTest',
  TestNames: {
    ElementDisplays: 'Element displays',
  },
};

Object.assign(
    window, {printer_setup_info_cros_test: printer_setup_info_cros_test});

suite(printer_setup_info_cros_test.suiteName, function() {
  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /** Verifies element can be added to UI and display. */
  test(printer_setup_info_cros_test.TestNames.ElementDisplays, function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const setupInfoElement =
        document.createElement(PrintPreviewPrinterSetupInfoCrosElement.is);
    document.body.appendChild(setupInfoElement);
    flush();

    assertTrue(!!setupInfoElement);
  });
});
