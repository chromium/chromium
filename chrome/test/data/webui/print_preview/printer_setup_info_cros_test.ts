// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrintPreviewPrinterSetupInfoCrosElement} from 'chrome://print/print_preview.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

const printer_setup_info_cros_test = {
  suiteName: 'PrinterSetupInfoCrosTest',
  TestNames: {
    ElementDisplays: 'Element displays',
    ElementLocalized: 'Element strings localized',
  },
};

Object.assign(
    window, {printer_setup_info_cros_test: printer_setup_info_cros_test});

suite(printer_setup_info_cros_test.suiteName, function() {
  let setupInfoElement: PrintPreviewPrinterSetupInfoCrosElement;

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /** Appends `PrintPreviewPrinterSetupInfoCrosElement` to document body. */
  function setupElement(): void {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setupInfoElement =
        document.createElement(PrintPreviewPrinterSetupInfoCrosElement.is);
    document.body.appendChild(setupInfoElement);
    flush();
  }

  /** Verifies element can be added to UI and display. */
  test(printer_setup_info_cros_test.TestNames.ElementDisplays, function() {
    setupElement();

    assertTrue(!!setupInfoElement);
    assertTrue(isChildVisible(setupInfoElement, 'cr-button'));
  });

  /** Verifies text is localized. */
  test(printer_setup_info_cros_test.TestNames.ElementLocalized, function() {
    setupElement();

    const managePrintersLabelKey = 'managePrintersLabel';
    assertTrue(setupInfoElement.i18nExists(managePrintersLabelKey));
    const managePrintersButton =
        setupInfoElement.shadowRoot!.querySelector<CrButtonElement>(
            'cr-button')!;
    assertEquals(
        setupInfoElement.i18n(managePrintersLabelKey),
        managePrintersButton.textContent!.trim());
  });
});
