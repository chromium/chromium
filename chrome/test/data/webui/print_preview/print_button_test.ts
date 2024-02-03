// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement, NativeInitialSettings, PrintPreviewAppElement, PrintTicket} from 'chrome://print/print_preview.js';
import {
  // <if expr="is_chromeos">
  GooglePromotedDestinationId,
  // </if>
  NativeLayerImpl, PluginProxyImpl} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// <if expr="is_chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

import {NativeLayerStub} from './native_layer_stub.js';
import {getDefaultInitialSettings} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

suite('PrintButtonTest', function() {
  let page: PrintPreviewAppElement;

  let nativeLayer: NativeLayerStub;

  let printBeforePreviewReady: boolean = false;

  let previewHidden: boolean = false;

  const initialSettings: NativeInitialSettings = getDefaultInitialSettings();

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="is_chromeos">
    setNativeLayerCrosInstance();
    // </if>
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    nativeLayer.setInitialSettings(initialSettings);
    const localDestinationInfos = [
      {printerName: 'FooName', deviceName: 'FooDevice'},
    ];
    nativeLayer.setLocalDestinations(localDestinationInfos);

    const pluginProxy = new TestPluginProxy();
    PluginProxyImpl.setInstance(pluginProxy);

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    pluginProxy.setPreloadCallback(() => {
      // Print before calling previewArea.onPluginLoadComplete_(). This
      // simulates the user clicking the print button while the preview is still
      // loading, since previewArea.onPluginLoadComplete_() indicates to the UI
      // that the preview is ready.
      if (printBeforePreviewReady) {
        const sidebar =
            page.shadowRoot!.querySelector('print-preview-sidebar')!;
        const parentElement =
            sidebar.shadowRoot!.querySelector('print-preview-button-strip')!;
        const printButton =
            parentElement.shadowRoot!.querySelector<CrButtonElement>(
                '.action-button')!;
        assertFalse(printButton.disabled);
        printButton.click();
      }
    });

    previewHidden = false;
    nativeLayer.whenCalled('hidePreview').then(() => {
      previewHidden = true;
    });
  });

  function waitForInitialPreview(): Promise<any> {
    return Promise.all([
      nativeLayer.whenCalled('getInitialSettings'),
      nativeLayer.whenCalled('getPrinterCapabilities'),
      nativeLayer.whenCalled('getPreview'),
    ]);
  }

  // Tests that hidePreview() is called before doPrint() if a local printer is
  // selected and the user clicks print while the preview is loading.
  test('LocalPrintHidePreview', function() {
    printBeforePreviewReady = true;

    return waitForInitialPreview()
        .then(function() {
          // Wait for the print request.
          return nativeLayer.whenCalled('doPrint');
        })
        .then(function(printTicket: string) {
          assertTrue(previewHidden);

          // Verify that the printer name is correct.
          assertEquals(
              'FooDevice', (JSON.parse(printTicket) as PrintTicket).deviceName);
          return nativeLayer.whenCalled('dialogClose');
        });
  });

  // Tests that hidePreview() is not called if Save as PDF is selected and
  // the user clicks print while the preview is loading.
  test('PDFPrintVisiblePreview', function() {
    printBeforePreviewReady = false;

    return waitForInitialPreview()
        .then(function() {
          nativeLayer.reset();
          // Setup to print before the preview loads.
          printBeforePreviewReady = true;

          // Select Save as PDF destination
          const destinationSettings =
              page.shadowRoot!.querySelector('print-preview-sidebar')!
                  .shadowRoot!.querySelector(
                      'print-preview-destination-settings')!;
          const pdfDestination =
              destinationSettings.getDestinationStoreForTest()
                  .destinations()
                  .find(d => d.id === 'Save as PDF');
          assertTrue(!!pdfDestination);
          destinationSettings.getDestinationStoreForTest().selectDestination(
              pdfDestination!);

          // Reload preview and wait for print.
          return nativeLayer.whenCalled('doPrint');
        })
        .then(function(printTicket) {
          assertFalse(previewHidden);

          // Verify that the printer name is correct.
          assertEquals(
              'Save as PDF',
              (JSON.parse(printTicket) as PrintTicket).deviceName);
          return nativeLayer.whenCalled('dialogClose');
        });
  });

  // <if expr="is_chromeos">
  // Tests that hidePreview() is not called if Save to Drive is selected on
  // Chrome OS and the user clicks print while the preview is loading because
  // Save to Drive needs to be treated like Save as PDF.
  test(
      'SaveToDriveVisiblePreviewCros', function() {
        printBeforePreviewReady = false;

        return waitForInitialPreview()
            .then(function() {
              nativeLayer.reset();
              // Setup to print before the preview loads.
              printBeforePreviewReady = true;

              // Select Save as PDF destination
              const destinationSettings =
                  page.shadowRoot!.querySelector('print-preview-sidebar')!
                      .shadowRoot!.querySelector(
                          'print-preview-destination-settings')!;
              const driveDestination =
                  destinationSettings.getDestinationStoreForTest()
                      .destinations()
                      .find(
                          d => d.id ===
                              GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS);
              assertTrue(!!driveDestination);
              destinationSettings.getDestinationStoreForTest()
                  .selectDestination(driveDestination!);

              // Reload preview and wait for print.
              return nativeLayer.whenCalled('doPrint');
            })
            .then(function(printTicket) {
              assertFalse(previewHidden);

              // Verify that the printer name is correct.
              assertEquals(
                  GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS,
                  (JSON.parse(printTicket) as PrintTicket).deviceName);
              return nativeLayer.whenCalled('dialogClose');
            });
      });
  // </if>
});
