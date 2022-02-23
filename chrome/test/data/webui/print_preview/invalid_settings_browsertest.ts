// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterfaceEventType, CloudPrintInterfaceImpl, CrButtonElement, Destination, DestinationStoreEventType, LocalDestinationInfo, makeRecentDestination, MeasurementSystemUnitType, NativeInitialSettings, NativeLayerImpl, PluginProxyImpl, PrintPreviewAppElement, ScalingType, State, whenReady} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, waitBeforeNextRender} from 'chrome://webui-test/test_util.js';

import {CloudPrintInterfaceStub} from './cloud_print_interface_stub.js';
// <if expr="chromeos_ash or chromeos_lacros">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationWithCertificateStatus, getCddTemplate, getDefaultMediaSize, getDefaultOrientation} from './print_preview_test_utils.js';
import {TestPluginProxy} from './test_plugin_proxy.js';

const invalid_settings_browsertest = {
  suiteName: 'InvalidSettingsBrowserTest',
  TestNames: {
    InvalidSettingsError: 'invalid settings error',
    InvalidCertificateError: 'invalid certificate error',
    InvalidCertificateErrorReselectDestination: 'invalid certificate reselect',
  },
};

Object.assign(
    window, {invalid_settings_browsertest: invalid_settings_browsertest});

suite(invalid_settings_browsertest.suiteName, function() {
  let page: PrintPreviewAppElement;

  let nativeLayer: NativeLayerStub;

  let cloudPrintInterface: CloudPrintInterfaceStub;

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
    serializedDefaultDestinationSelectionRulesStr: null
  };

  let localDestinationInfos: LocalDestinationInfo[] = [
    {printerName: 'FooName', deviceName: 'FooDevice'},
    {printerName: 'BarName', deviceName: 'BarDevice'},
  ];

  setup(function() {
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="chromeos_ash or chromeos_lacros">
    setNativeLayerCrosInstance();
    // </if>
    cloudPrintInterface = new CloudPrintInterfaceStub();
    CloudPrintInterfaceImpl.setInstance(cloudPrintInterface);
    document.body.innerHTML = '';
  });

  /**
   * Initializes the page with initial settings and local destinations
   * given by |initialSettings| and |localDestinationInfos|. Also creates
   * the fake plugin. Moved out of setup so tests can set those parameters
   * differently.
   */
  function createPage() {
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(localDestinationInfos);
    PluginProxyImpl.setInstance(new TestPluginProxy());

    page = document.createElement('print-preview-app');
    document.body.appendChild(page);
    page.$.documentInfo.init(true, false, 'title', false);
  }

  /**
   * Performs some setup for invalid certificate tests using 2 destinations
   * in |printers|. printers[0] will be set as the most recent destination,
   * and printers[1] will be the second most recent destination. Sets up
   * cloud print interface, user info, and runs createPage().
   */
  function setupInvalidCertificateTest(printers: Destination[]) {
    initialSettings.printerName = '';
    initialSettings.serializedAppStateStr = JSON.stringify({
      version: 2,
      recentDestinations: [
        makeRecentDestination(printers[0]!),
        makeRecentDestination(printers[1]!),
      ],
    });
    initialSettings.cloudPrintURL = 'cloudprint URL';
    localDestinationInfos = [];

    loadTimeData.overrideValues({isEnterpriseManaged: false});
    createPage();

    printers.forEach(printer => cloudPrintInterface.setPrinter(printer));
  }

  // Tests that when a printer cannot be communicated with correctly the
  // preview area displays an invalid printer error message and printing
  // is disabled. Verifies that the user can recover from this error by
  // selecting a different, valid printer.
  test(
      assert(invalid_settings_browsertest.TestNames.InvalidSettingsError),
      function() {
        createPage();
        const barDevice = getCddTemplate('BarDevice');
        nativeLayer.setLocalDestinationCapabilities(barDevice);

        // FooDevice is the default printer, so will be selected for the initial
        // preview request.
        nativeLayer.setInvalidPrinterId('FooDevice');

        // Expected message
        const expectedMessage =
            'The selected printer is not available or not ' +
            'installed correctly.  Check your printer or try selecting another ' +
            'printer.';

        // Get references to relevant elements.
        const previewAreaEl = page.$.previewArea;
        const overlay = previewAreaEl.shadowRoot!.querySelector(
            '.preview-area-overlay-layer')!;
        const messageEl =
            previewAreaEl.shadowRoot!.querySelector('.preview-area-message')!;
        const sidebar =
            page.shadowRoot!.querySelector('print-preview-sidebar')!;
        let printButton: CrButtonElement;
        const destinationSettings = sidebar.shadowRoot!.querySelector(
            'print-preview-destination-settings')!;

        return waitBeforeNextRender(page)
            .then(() => {
              const parentElement = sidebar.shadowRoot!.querySelector(
                  'print-preview-button-strip')!;
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
                nativeLayer.whenCalled('getPreview')
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
              destinationSettings.getDestinationStoreForTest()
                  .selectDestination(assert(barDestination!));

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
              return nativeLayer.whenCalled('print');
            })
            .then(
                /**
                 * @param {string} printTicket The print ticket print() was
                 *     called for.
                 */
                function(printTicket) {
                  // Sanity check some printing argument values.
                  const ticket = JSON.parse(printTicket);
                  assertEquals(
                      barDevice.printer!.deviceName, ticket.deviceName);
                  assertEquals(
                      getDefaultOrientation(barDevice) === 'LANDSCAPE',
                      ticket.landscape);
                  assertEquals(1, ticket.copies);
                  const mediaDefault = getDefaultMediaSize(barDevice);
                  assertEquals(
                      mediaDefault.width_microns,
                      ticket.mediaSize.width_microns);
                  assertEquals(
                      mediaDefault.height_microns,
                      ticket.mediaSize.height_microns);
                  return nativeLayer.whenCalled('dialogClose');
                });
      });

  // Test that GCP invalid certificate printers disable the print preview when
  // selected and display an error and that the preview dialog can be
  // recovered by selecting a new destination. Verifies this works when the
  // invalid printer is the most recent destination and is selected by
  // default.
  test(
      assert(invalid_settings_browsertest.TestNames.InvalidCertificateError),
      function() {
        const invalidPrinter = createDestinationWithCertificateStatus(
            'FooDevice', 'FooName', true);
        const validPrinter = createDestinationWithCertificateStatus(
            'BarDevice', 'BarName', false);
        setupInvalidCertificateTest([invalidPrinter, validPrinter]);

        // Expected message
        const expectedMessageStart = 'The selected Google Cloud Print device ' +
            'is no longer supported.';
        const expectedMessageEnd = 'Try setting up the printer in your ' +
            'computer\'s system settings.';

        // Get references to relevant elements.
        const previewAreaEl = page.$.previewArea;
        const overlayEl = previewAreaEl.shadowRoot!.querySelector(
            '.preview-area-overlay-layer')!;
        const messageEl =
            previewAreaEl.shadowRoot!.querySelector('.preview-area-message')!;
        const sidebar =
            page.shadowRoot!.querySelector('print-preview-sidebar')!;
        let printButton: CrButtonElement;
        const destinationSettings = sidebar.shadowRoot!.querySelector(
            'print-preview-destination-settings')!;
        const scalingSettings =
            sidebar.shadowRoot!.querySelector('print-preview-scaling-settings')!
                .shadowRoot!.querySelector(
                    'print-preview-number-settings-section')!;
        const layoutSettings =
            sidebar.shadowRoot!.querySelector('print-preview-layout-settings')!;

        return waitBeforeNextRender(page)
            .then(() => {
              const parentElement = sidebar.shadowRoot!.querySelector(
                  'print-preview-button-strip')!;
              printButton =
                  parentElement.shadowRoot!.querySelector<CrButtonElement>(
                      '.action-button')!;
              return Promise.all([
                whenReady(),
                nativeLayer.whenCalled('getInitialSettings'),
              ]);
            })
            .then(function() {
              // Set this to enable the scaling input.
              page.setSetting('scalingType', ScalingType.CUSTOM);

              destinationSettings.getDestinationStoreForTest()
                  .startLoadCloudDestinations();

              return eventToPromise(
                  CloudPrintInterfaceEventType.PRINTER_DONE,
                  cloudPrintInterface.getEventTarget());
            })
            .then(function() {
              // FooDevice will be selected since it is the most recently used
              // printer, so the invalid certificate error should be shown.
              // The overlay must be visible for the message to be seen.
              assertFalse(overlayEl.classList.contains('invisible'));

              // Verify that the correct message is shown.
              assertTrue(messageEl.textContent!.includes(expectedMessageStart));
              assertTrue(messageEl.textContent!.includes(expectedMessageEnd));

              // Verify that the print button is disabled
              assertTrue(printButton.disabled);

              // Verify the state is invalid and that some settings sections are
              // also disabled, so there is no way to regenerate the preview.
              assertEquals(State.ERROR, page.state);
              assertTrue(
                  layoutSettings.shadowRoot!.querySelector('select')!.disabled);
              assertTrue(scalingSettings.shadowRoot!.querySelector(
                                                        'cr-input')!.disabled);

              // The destination select dropdown should be enabled, so that the
              // user can select a new printer.
              assertFalse(destinationSettings.$.destinationSelect.disabled);

              // Reset
              nativeLayer.reset();

              // Select a new, valid cloud destination.
              destinationSettings.getDestinationStoreForTest()
                  .selectDestination(validPrinter);

              return nativeLayer.whenCalled('getPreview');
            })
            .then(function() {
              // Has active print button, indicating recovery from error state.
              assertFalse(printButton.disabled);
              assertEquals(State.READY, page.state);

              // Settings sections are now active.
              assertFalse(
                  layoutSettings.shadowRoot!.querySelector('select')!.disabled);
              assertFalse(scalingSettings.shadowRoot!.querySelector(
                                                         'cr-input')!.disabled);

              // The destination select dropdown should still be enabled.
              assertFalse(destinationSettings.$.destinationSelect.disabled);

              // Message text should have changed and overlay should be
              // invisible.
              assertFalse(
                  messageEl.textContent!.includes(expectedMessageStart));
              assertTrue(overlayEl.classList.contains('invisible'));
            });
      });

  // Test that GCP invalid certificate printers disable the print preview when
  // selected and display an error and that the preview dialog can be
  // recovered by selecting a new destination. Tests that even if destination
  // was previously selected, the error is cleared.
  test(
      assert(invalid_settings_browsertest.TestNames
                 .InvalidCertificateErrorReselectDestination),
      function() {
        const invalidPrinter = createDestinationWithCertificateStatus(
            'FooDevice', 'FooName', true);
        const validPrinter = createDestinationWithCertificateStatus(
            'BarDevice', 'BarName', false);
        setupInvalidCertificateTest([validPrinter, invalidPrinter]);

        // Get references to relevant elements.
        const previewAreaEl = page.$.previewArea;
        const overlayEl = previewAreaEl.shadowRoot!.querySelector(
            '.preview-area-overlay-layer')!;
        const sidebar =
            page.shadowRoot!.querySelector('print-preview-sidebar')!;
        let printButton: CrButtonElement;
        const destinationSettings = sidebar.shadowRoot!.querySelector(
            'print-preview-destination-settings')!;

        return waitBeforeNextRender(page)
            .then(() => {
              const parentElement = sidebar.shadowRoot!.querySelector(
                  'print-preview-button-strip')!;
              printButton =
                  parentElement.shadowRoot!.querySelector<CrButtonElement>(
                      '.action-button')!;
              return Promise.all([
                whenReady(),
                nativeLayer.whenCalled('getInitialSettings'),
              ]);
            })
            .then(function() {
              // Start loading cloud destinations so that the printer
              // capabilities arrive.
              destinationSettings.getDestinationStoreForTest()
                  .startLoadCloudDestinations();
              return nativeLayer.whenCalled('getPreview');
            })
            .then(function() {
              // Has active print button.
              assertFalse(printButton.disabled);
              assertEquals(State.READY, page.state);
              // No error message.
              assertTrue(overlayEl.classList.contains('invisible'));

              // Select the invalid destination and wait for the event.
              const whenInvalid = eventToPromise(
                  DestinationStoreEventType.ERROR,
                  destinationSettings.getDestinationStoreForTest());
              destinationSettings.getDestinationStoreForTest()
                  .selectDestination(invalidPrinter);
              return whenInvalid;
            })
            .then(function() {
              // Should have error message.
              assertFalse(overlayEl.classList.contains('invisible'));
              assertEquals(State.ERROR, page.state);

              // Reset
              nativeLayer.reset();

              // Reselect the valid cloud destination.
              const whenSelected = eventToPromise(
                  DestinationStoreEventType.DESTINATION_SELECT,
                  destinationSettings.getDestinationStoreForTest());
              destinationSettings.getDestinationStoreForTest()
                  .selectDestination(validPrinter);
              return whenSelected;
            })
            .then(function() {
              // Has active print button and no error message.
              assertFalse(printButton.disabled);
              assertTrue(overlayEl.classList.contains('invisible'));
            });
      });
});
