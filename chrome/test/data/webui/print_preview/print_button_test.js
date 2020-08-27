// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, NativeLayer, NativeLayerImpl, PluginProxyImpl} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {getDefaultInitialSettings} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {TestPluginProxy} from 'chrome://test/print_preview/test_plugin_proxy.js';

window.print_button_test = {};
print_button_test.suiteName = 'PrintButtonTest';
/** @enum {string} */
print_button_test.TestNames = {
  LocalPrintHidePreview: 'local print hide preview',
  PDFPrintVisiblePreview: 'pdf print visible preview',
  SaveToDriveVisiblePreviewCros: 'save to drive visible preview cros',
};

suite(print_button_test.suiteName, function() {
  /** @type {?PrintPreviewAppElement} */
  let page = null;

  /** @type {?NativeLayer} */
  let nativeLayer = null;

  /** @type {boolean} */
  let printBeforePreviewReady = false;

  /** @type {boolean} */
  let previewHidden = false;

  /** @type {!NativeInitialSettings} */
  const initialSettings = getDefaultInitialSettings();

  /** @override */
  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    document.body.innerHTML = '';
    nativeLayer.setInitialSettings(initialSettings);
    const localDestinationInfos = [
      {printerName: 'FooName', deviceName: 'FooDevice'},
    ];
    nativeLayer.setLocalDestinations(localDestinationInfos);

    const pluginProxy = new TestPluginProxy();
    pluginProxy.setPluginCompatible(true);
    PluginProxyImpl.instance_ = pluginProxy;

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    pluginProxy.setPreloadCallback(() => {
      // Print before calling previewArea.onPluginLoadComplete_(). This
      // simulates the user clicking the print button while the preview is still
      // loading, since previewArea.onPluginLoadComplete_() indicates to the UI
      // that the preview is ready.
      if (printBeforePreviewReady) {
        const sidebar = page.$$('print-preview-sidebar');
        const parentElement = sidebar.$$('print-preview-button-strip');
        const printButton = parentElement.$$('.action-button');
        assertFalse(printButton.disabled);
        printButton.click();
      }
    });

    previewHidden = false;
    nativeLayer.whenCalled('hidePreview').then(() => {
      previewHidden = true;
    });
  });

  function waitForInitialPreview() {
    return nativeLayer.whenCalled('getInitialSettings').then(function() {
      // Wait for the preview request.
      return Promise.all([
        nativeLayer.whenCalled('getPrinterCapabilities'),
        nativeLayer.whenCalled('getPreview')
      ]);
    });
  }

  // Tests that hidePreview() is called before print() if a local printer is
  // selected and the user clicks print while the preview is loading.
  test(assert(print_button_test.TestNames.LocalPrintHidePreview), function() {
    printBeforePreviewReady = true;

    return waitForInitialPreview()
        .then(function() {
          // Wait for the print request.
          return nativeLayer.whenCalled('print');
        })
        .then(function(printTicket) {
          assertTrue(previewHidden);

          // Verify that the printer name is correct.
          assertEquals('FooDevice', JSON.parse(printTicket).deviceName);
          return nativeLayer.whenCalled('dialogClose');
        });
  });

  // Tests that hidePreview() is not called if Save as PDF is selected and
  // the user clicks print while the preview is loading.
  test(assert(print_button_test.TestNames.PDFPrintVisiblePreview), function() {
    printBeforePreviewReady = false;

    return waitForInitialPreview()
        .then(function() {
          nativeLayer.reset();
          // Setup to print before the preview loads.
          printBeforePreviewReady = true;

          // Select Save as PDF destination
          const destinationSettings =
              page.$$('print-preview-sidebar')
                  .$$('print-preview-destination-settings');
          const pdfDestination =
              destinationSettings.destinationStore_.destinations().find(
                  d => d.id === 'Save as PDF');
          assertTrue(!!pdfDestination);
          destinationSettings.destinationStore_.selectDestination(
              pdfDestination);

          // Reload preview and wait for print.
          return nativeLayer.whenCalled('print');
        })
        .then(function(printTicket) {
          assertFalse(previewHidden);

          // Verify that the printer name is correct.
          assertEquals('Save as PDF', JSON.parse(printTicket).deviceName);
          return nativeLayer.whenCalled('dialogClose');
        });
  });

  // Tests that hidePreview() is not called if Save to Drive is selected on
  // Chrome OS and the user clicks print while the preview is loading because
  // when the flag |kPrintSaveToDrive| is enabled, Save to Drive needs to be
  // treated like Save as PDF.
  test(
      assert(print_button_test.TestNames.SaveToDriveVisiblePreviewCros),
      function() {
        printBeforePreviewReady = false;

        return waitForInitialPreview()
            .then(function() {
              nativeLayer.reset();
              // Setup to print before the preview loads.
              printBeforePreviewReady = true;

              // Select Save as PDF destination
              const destinationSettings =
                  page.$$('print-preview-sidebar')
                      .$$('print-preview-destination-settings');
              const driveDestination =
                  destinationSettings.destinationStore_.destinations().find(
                      d => d.id ===
                          Destination.GooglePromotedId.SAVE_TO_DRIVE_CROS);
              assertTrue(!!driveDestination);
              destinationSettings.destinationStore_.selectDestination(
                  driveDestination);

              // Reload preview and wait for print.
              return nativeLayer.whenCalled('print');
            })
            .then(function(printTicket) {
              assertFalse(previewHidden);

              // Verify that the printer name is correct.
              assertEquals(
                  Destination.GooglePromotedId.SAVE_TO_DRIVE_CROS,
                  JSON.parse(printTicket).deviceName);
              return nativeLayer.whenCalled('dialogClose');
            });
      });
});
