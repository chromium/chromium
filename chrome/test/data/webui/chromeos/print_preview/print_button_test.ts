// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement, NativeInitialSettings, PrintPreviewAppElement, PrintTicket} from 'chrome://print/print_preview.js';
import {
  // <if expr="is_chromeos">
  GooglePromotedDestinationId,
  // </if>
  NativeLayerImpl, PluginProxyImpl, State} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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

  let cancelBeforePreviewReady: boolean = false;

  let previewHidden: boolean = false;

  let stateLog: State[] = [];

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

    stateLog = [];
    page.$.state.addEventListener('state-changed', e => {
      const newState = (e as CustomEvent<{value: State}>).detail.value;
      stateLog.push(newState);
    });

    pluginProxy.setPreloadCallback(() => {
      // Print before calling previewArea.onPluginLoadComplete_(). This
      // simulates the user clicking the print button while the preview is still
      // loading, since previewArea.onPluginLoadComplete_() indicates to the UI
      // that the preview is ready.
      const sidebar = page.$.sidebar;
      const buttonStrip =
          sidebar.shadowRoot!.querySelector('print-preview-button-strip');
      assertTrue(!!buttonStrip);
      if (printBeforePreviewReady) {
        const printButton =
            buttonStrip.shadowRoot!.querySelector<CrButtonElement>(
                '.action-button');
        assertTrue(!!printButton);
        assertFalse(printButton.disabled);
        printButton.click();
      }
      if (cancelBeforePreviewReady) {
        flush();
        const cancelButton =
            buttonStrip.shadowRoot!.querySelector<CrButtonElement>(
                '.cancel-button');
        assertTrue(!!cancelButton);
        assertFalse(cancelButton.disabled);
        cancelButton.click();
      }
    });

    previewHidden = false;
    nativeLayer.whenCalled('hidePreview').then(() => {
      previewHidden = true;
    });
  });

  teardown(function() {
    // Reset a couple of globals.
    printBeforePreviewReady = false;
    cancelBeforePreviewReady = false;
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
  test('LocalPrintHidePreview', async () => {
    printBeforePreviewReady = true;

    await waitForInitialPreview();
    const printTicket = await nativeLayer.whenCalled('doPrint');
    assertTrue(previewHidden);

    // Verify that the printer name is correct.
    assertEquals(
        'FooDevice', (JSON.parse(printTicket) as PrintTicket).deviceName);
    const cancelled = await nativeLayer.whenCalled('dialogClose');
    assertFalse(cancelled);

    // Verify state transitions.
    const expectedStates = [
      State.READY,
      State.PRINT_PENDING,
      State.HIDDEN,
      State.PRINTING,
      State.CLOSING,
    ];
    assertDeepEquals(expectedStates, stateLog);
  });

  function selectPdfDestination() {
    // Selects the Save as PDF destination.
    const destinationSettings = page.$.sidebar.$.destinationSettings;
    const pdfDestination =
        destinationSettings.getDestinationStoreForTest().destinations().find(
            d => d.id === 'Save as PDF');
    assertTrue(!!pdfDestination);
    destinationSettings.getDestinationStoreForTest().selectDestination(
        pdfDestination);
  }

  // Tests that hidePreview() is not called if Save as PDF is selected and
  // the user clicks print while the preview is loading.
  test('PDFPrintVisiblePreview', async () => {
    await waitForInitialPreview();
    nativeLayer.reset();

    // Setup to print before the preview loads and select the Save as PDF
    // printer.
    printBeforePreviewReady = true;
    selectPdfDestination();

    // Reload preview and wait for print.
    const printTicket = await nativeLayer.whenCalled('doPrint');
    assertFalse(previewHidden);

    // Verify that the printer name is correct.
    assertEquals(
        'Save as PDF', (JSON.parse(printTicket) as PrintTicket).deviceName);
    const cancelled = await nativeLayer.whenCalled('dialogClose');
    assertFalse(cancelled);

    // Verify state transitions.
    const expectedStates = [
      State.READY,
      State.NOT_READY,
      State.READY,
      State.PRINT_PENDING,
      State.PRINTING,
      State.CLOSING,
    ];
    assertDeepEquals(expectedStates, stateLog);
  });

  // Tests that the preview can be cancelled if Save as PDF is selected and the
  // user clicks print while the preview is loading.
  // Regression test for crbug.com/40800893.
  test('PDFPrintCancelPreview', async () => {
    await waitForInitialPreview();
    nativeLayer.reset();
    // Setup to print and then cancel before the preview loads and
    // select the Save as PDF destination.
    printBeforePreviewReady = true;
    cancelBeforePreviewReady = true;
    selectPdfDestination();
    // Dialog should close successfully.
    const cancelled = await nativeLayer.whenCalled('dialogClose');
    assertTrue(cancelled);

    // Verify state transitions.
    const expectedStates = [
      State.READY,
      State.NOT_READY,
      State.READY,
      State.PRINT_PENDING,
      State.CLOSING,
    ];
    assertDeepEquals(expectedStates, stateLog);
  });

  // <if expr="is_chromeos">
  // Tests that hidePreview() is not called if Save to Drive is selected on
  // Chrome OS and the user clicks print while the preview is loading because
  // Save to Drive needs to be treated like Save as PDF.
  test('SaveToDriveVisiblePreviewCros', async () => {
    await waitForInitialPreview();
    nativeLayer.reset();
    // Setup to print before the preview loads.
    printBeforePreviewReady = true;

    // Select Save to Drive destination
    const destinationSettings = page.$.sidebar.$.destinationSettings;
    const driveDestination =
        destinationSettings.getDestinationStoreForTest().destinations().find(
            d => d.id === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS);
    assertTrue(!!driveDestination);
    destinationSettings.getDestinationStoreForTest().selectDestination(
        driveDestination);

    // Reload preview and wait for print.
    const printTicket = await nativeLayer.whenCalled('doPrint');
    assertFalse(previewHidden);

    // Verify that the printer name is correct.
    assertEquals(
        GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS,
        (JSON.parse(printTicket) as PrintTicket).deviceName);
    const cancelled = await nativeLayer.whenCalled('dialogClose');
    assertFalse(cancelled);

    // Verify state transitions.
    const expectedStates = [
      State.READY,
      State.NOT_READY,
      State.READY,
      State.PRINT_PENDING,
      State.PRINTING,
      State.CLOSING,
    ];
    assertDeepEquals(expectedStates, stateLog);
  });
  // </if>
});
