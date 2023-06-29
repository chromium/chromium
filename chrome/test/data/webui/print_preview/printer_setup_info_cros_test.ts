// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeLayerImpl, PrintPreviewPrinterSetupInfoCrosElement} from 'chrome://print/print_preview.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';

const printer_setup_info_cros_test = {
  suiteName: 'PrinterSetupInfoCrosTest',
  TestNames: {
    ElementDisplays: 'Element displays',
    ElementLocalized: 'Element strings localized',
    ManagePrintersButton: 'Manage printers button launches settings',
  },
};

Object.assign(
    window, {printer_setup_info_cros_test: printer_setup_info_cros_test});

suite(printer_setup_info_cros_test.suiteName, function() {
  let setupInfoElement: PrintPreviewPrinterSetupInfoCrosElement;
  let nativeLayer: NativeLayerStub;

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
  });

  teardown(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /** @return {T} Element from shadow root */
  function getShadowElement<T extends HTMLElement>(
      parentElement: PolymerElement, selector: string): T {
    assertTrue(!!parentElement);
    const element = parentElement.shadowRoot!.querySelector(selector) as T;
    assertTrue(!!element);
    return element;
  }

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
        getShadowElement<CrButtonElement>(setupInfoElement, 'cr-button');
    assertEquals(
        setupInfoElement.i18n(managePrintersLabelKey),
        managePrintersButton.textContent!.trim());
  });

  /**
   * Verifies manage printers button invokes launch settings from native layer.
   */
  test(printer_setup_info_cros_test.TestNames.ManagePrintersButton, function() {
    setupElement();
    assertEquals(0, nativeLayer.getCallCount('managePrinters'));

    // Click button.
    const managePrinters =
        getShadowElement<CrButtonElement>(setupInfoElement, 'cr-button');
    managePrinters.click();

    assertEquals(1, nativeLayer.getCallCount('managePrinters'));
  });
});
