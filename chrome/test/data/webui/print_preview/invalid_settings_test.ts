// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement, Destination, NativeInitialSettings, PrintPreviewAppElement} from 'chrome://print/print_preview.js';
import {MeasurementSystemUnitType, NativeLayerImpl, PluginProxyImpl, State, whenReady} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

// <if expr="is_chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>
import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate, getDefaultMediaSize, getDefaultOrientation} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

suite('InvalidSettingsTest', function() {
  let page: PrintPreviewAppElement;

  let nativeLayer: NativeLayerStub;

  const initialSettings: NativeInitialSettings = {
    isInKioskAutoPrintMode: false,
    isInAppKioskMode: false,
    thousandsDelimiter: ',',
    decimalDelimiter: '.',
    unitType: MeasurementSystemUnitType.IMPERIAL,
    previewModifiable: true,
    destinationsManaged: false,
    previewIsFromArc: false,
    documentTitle: 'title',
    documentHasSelection: true,
    shouldPrintSelectionOnly: false,
    uiLocale: 'en-us',
    printerName: 'FooDevice',
    pdfPrinterDisabled: false,
    serializedAppStateStr: null,
    serializedDefaultDestinationSelectionRulesStr: null,
  };

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="is_chromeos">
    setNativeLayerCrosInstance();
    // </if>
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /**
   * Initializes the page with initial settings and local destinations
   * given by |initialSettings| and |localDestinationInfos|. Also creates
   * the fake plugin. Moved out of setup so tests can set those parameters
   * differently.
   */
  function createPage() {
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations([
      {printerName: 'FooName', deviceName: 'FooDevice'},
      {printerName: 'BarName', deviceName: 'BarDevice'},
    ]);
    PluginProxyImpl.setInstance(new TestPluginProxy());

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    page.$.documentInfo.init(true, false, 'title', false);
  }

  // Tests that when a printer cannot be communicated with correctly the
  // preview area displays an invalid printer error message and printing
  // is disabled. Verifies that the user can recover from this error by
  // selecting a different, valid printer.
  test('invalid settings error', function() {
    createPage();
    const barDevice = getCddTemplate('BarDevice');
    nativeLayer.setLocalDestinationCapabilities(barDevice);

    // FooDevice is the default printer, so will be selected for the initial
    // preview request.
    nativeLayer.setInvalidPrinterId('FooDevice');

    // Expected message
    const expectedMessage = 'The selected printer is not available or not ' +
        'installed correctly.  Check your printer or try selecting another ' +
        'printer.';

    // Get references to relevant elements.
    const previewAreaEl = page.$.previewArea;
    const overlay =
        previewAreaEl.shadowRoot!.querySelector('.preview-area-overlay-layer')!;
    const messageEl =
        previewAreaEl.shadowRoot!.querySelector('.preview-area-message')!;
    const sidebar = page.shadowRoot!.querySelector('print-preview-sidebar')!;
    let printButton: CrButtonElement;
    const destinationSettings = sidebar.shadowRoot!.querySelector(
        'print-preview-destination-settings')!;

    return waitBeforeNextRender(page)
        .then(() => {
          const parentElement =
              sidebar.shadowRoot!.querySelector('print-preview-button-strip')!;
          printButton =
              parentElement.shadowRoot!.querySelector<CrButtonElement>(
                  '.action-button')!;

          return Promise.all([
            whenReady(),
            nativeLayer.whenCalled('getInitialSettings'),
          ]);
        })
        .then(function() {
          destinationSettings.getDestinationStoreForTest()
              .startLoadAllDestinations();
          // Wait for the preview request.
          return Promise.all([
            nativeLayer.whenCalled('getPrinterCapabilities'),
            nativeLayer.whenCalled('getPreview'),
          ]);
        })
        .then(function() {
          // Print preview should have failed with invalid settings, since
          // FooDevice was set as an invalid printer.
          assertFalse(overlay.classList.contains('invisible'));
          assertTrue(messageEl.textContent!.includes(expectedMessage));
          assertEquals(State.ERROR, page.state);

          // Verify that the print button is disabled
          assertTrue(printButton.disabled);

          // Select should still be enabled so that the user can select a
          // new printer.
          assertFalse(destinationSettings.$.destinationSelect.disabled);

          // Reset
          nativeLayer.reset();

          // Select a new destination
          const barDestination =
              destinationSettings.getDestinationStoreForTest()
                  .destinations()
                  .find((d: Destination) => d.id === 'BarDevice');
          assert(barDestination);
          destinationSettings.getDestinationStoreForTest().selectDestination(
              barDestination);

          // Wait for the preview to be updated.
          return nativeLayer.whenCalled('getPreview');
        })
        .then(function() {
          // Message should be gone.
          assertTrue(overlay.classList.contains('invisible'));
          assertFalse(messageEl.textContent!.includes(expectedMessage));

          // Has active print button and successfully 'prints', indicating
          assertFalse(printButton.disabled);
          assertEquals(State.READY, page.state);
          printButton.click();
          // This should result in a call to print.
          return nativeLayer.whenCalled('doPrint');
        })
        .then(
            /**
             * @param {string} printTicket The print ticket print() was
             *     called for.
             */
            function(printTicket) {
              // Sanity check some printing argument values.
              const ticket = JSON.parse(printTicket);
              assertEquals(barDevice.printer!.deviceName, ticket.deviceName);
              assertEquals(
                  getDefaultOrientation(barDevice) === 'LANDSCAPE',
                  ticket.landscape);
              assertEquals(1, ticket.copies);
              const mediaDefault = getDefaultMediaSize(barDevice);
              assertEquals(
                  mediaDefault.width_microns, ticket.mediaSize.width_microns);
              assertEquals(
                  mediaDefault.height_microns, ticket.mediaSize.height_microns);
              return nativeLayer.whenCalled('dialogClose');
            });
  });
});
