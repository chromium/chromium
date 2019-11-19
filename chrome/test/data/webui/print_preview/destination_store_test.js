// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationErrorType, DestinationOrigin, DestinationStore, DestinationType, makeRecentDestination, NativeLayer, PluginProxy, PrinterType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {CloudPrintInterfaceStub} from 'chrome://test/print_preview/cloud_print_interface_stub.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {PDFPluginStub} from 'chrome://test/print_preview/plugin_stub.js';
import {createDestinationStore, createDestinationWithCertificateStatus, getCddTemplate, getDefaultInitialSettings, getDestinations, getGoogleDriveDestination, getSaveAsPdfDestination, setupTestListenerElement} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

window.destination_store_test = {};
destination_store_test.suiteName = 'DestinationStoreTest';
/** @enum {string} */
destination_store_test.TestNames = {
  SingleRecentDestination: 'single recent destination',
  MultipleRecentDestinations: 'multiple recent destinations',
  MultipleRecentDestinationsOneRequest:
      'multiple recent destinations one request',
  DefaultDestinationSelectionRules: 'default destination selection rules',
  SystemDefaultPrinterPolicy: 'system default printer policy',
  KioskModeSelectsFirstPrinter: 'kiosk mode selects first printer',
  NoPrintersShowsError: 'no printers shows error',
  UnreachableRecentCloudPrinter: 'unreachable recent cloud printer',
  RecentSaveAsPdf: 'recent save as pdf',
  MultipleRecentDestinationsAccounts: 'multiple recent destinations accounts',
  LoadAndSelectDestination: 'select loaded destination',
};

suite(destination_store_test.suiteName, function() {
  /** @type {?DestinationStore} */
  let destinationStore = null;

  /** @type {?NativeLayerStub} */
  let nativeLayer = null;

  /** @type {?cloudprint.CloudPrintInterface} */
  let cloudPrintInterface = null;

  /** @type {?NativeInitialSettngs} */
  let initialSettings = null;

  /** @type {!Array<!LocalDestinationInfo>} */
  let localDestinations = [];

  /** @type {!Array<!Destination>} */
  let cloudDestinations = [];

  /** @type {!Array<!Destination>} */
  let destinations = [];

  /** @type {number} */
  let numPrintersSelected = 0;

  /** @override */
  setup(function() {
    // Clear the UI.
    PolymerTest.clearBody();

    setupTestListenerElement();

    nativeLayer = new NativeLayerStub();
    NativeLayer.setInstance(nativeLayer);

    initialSettings = getDefaultInitialSettings();
    initialSettings.userAccounts = [];
    localDestinations = [];
    destinations = getDestinations(nativeLayer, localDestinations);
  });

  /*
   * Sets the initial settings to the stored value and creates the page.
   * @param {boolean=} opt_expectPrinterFailure Whether printer fetch is
   *     expected to fail
   * @return {!Promise} Promise that resolves when initial settings and,
   *     if printer failure is not expected, printer capabilities have
   *     been returned.
   */
  function setInitialSettings(opt_expectPrinterFailure) {
    // Set local print list.
    nativeLayer.setLocalDestinations(localDestinations);

    // Create cloud print interface.
    cloudPrintInterface = new CloudPrintInterfaceStub();
    cloudDestinations.forEach(cloudDestination => {
      cloudPrintInterface.setPrinter(cloudDestination);
    });

    // Create destination store.
    destinationStore = createDestinationStore();
    destinationStore.setCloudPrintInterface(cloudPrintInterface);
    destinationStore.addEventListener(
        DestinationStore.EventType.DESTINATION_SELECT, function() {
          numPrintersSelected++;
        });
    destinationStore.setActiveUser(
        initialSettings.userAccounts.length > 0 ?
            initialSettings.userAccounts[0] :
            '');

    // Initialize.
    const recentDestinations = initialSettings.serializedAppStateStr ?
        JSON.parse(initialSettings.serializedAppStateStr).recentDestinations :
        [];
    const whenCapabilitiesReady = eventToPromise(
        DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        destinationStore);
    destinationStore.init(
        initialSettings.pdfPrinterDisabled, initialSettings.printerName,
        initialSettings.serializedDefaultDestinationSelectionRulesStr,
        recentDestinations);
    return opt_expectPrinterFailure ? Promise.resolve() : Promise.race([
      nativeLayer.whenCalled('getPrinterCapabilities'), whenCapabilitiesReady
    ]);
  }

  /**
   * Tests that if the user has a single valid recent destination the
   * destination is automatically reselected.
   */
  test(
      assert(destination_store_test.TestNames.SingleRecentDestination),
      function() {
        const recentDestination = makeRecentDestination(destinations[0]);
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: [recentDestination],
        });

        return setInitialSettings().then(function(args) {
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL, args.type);
          assertEquals('ID1', destinationStore.selectedDestination.id);
        });
      });

  /**
   * Tests that if the user has multiple valid recent destinations the most
   * recent destination is automatically reselected and its capabilities are
   * fetched.
   */
  test(
      assert(destination_store_test.TestNames.MultipleRecentDestinations),
      function() {
        const recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        return setInitialSettings().then(function(args) {
          // Should have loaded ID1 as the selected printer, since it was most
          // recent.
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL, args.type);
          assertEquals('ID1', destinationStore.selectedDestination.id);
          // Only the most recent printer should have been added to the store.
          const reportedPrinters = destinationStore.destinations();
          destinations.forEach((destination, index) => {
            const match = reportedPrinters.find((reportedPrinter) => {
              return reportedPrinter.id == destination.id;
            });
            assertEquals(index > 0, typeof match === 'undefined');
          });
        });
      });

  /**
   * Tests that if the user has multiple valid recent destinations, this
   * does not result in multiple calls to getPrinterCapabilities and the
   * correct destination is selected for the preview request.
   * For crbug.com/666595.
   */
  test(
      assert(destination_store_test.TestNames
                 .MultipleRecentDestinationsOneRequest),
      function() {
        const recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        return setInitialSettings().then(function(args) {
          // Should have loaded ID1 as the selected printer, since it was most
          // recent.
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL, args.type);
          assertEquals('ID1', destinationStore.selectedDestination.id);

          // Most recent printer + Save as PDF are in the store automatically.
          const reportedPrinters = destinationStore.destinations();
          assertEquals(2, reportedPrinters.length);
          destinations.forEach((destination, index) => {
            assertEquals(
                index === 0,
                reportedPrinters.some(p => p.id == destination.id));
          });
          assertEquals(1, numPrintersSelected);
        });
      });

  /**
   * Tests that if there are default destination selection rules they are
   * respected and a matching destination is automatically selected.
   */
  test(
      assert(destination_store_test.TestNames.DefaultDestinationSelectionRules),
      function() {
        initialSettings.serializedDefaultDestinationSelectionRulesStr =
            JSON.stringify({namePattern: '.*Four.*'});
        initialSettings.serializedAppStateStr = '';
        return setInitialSettings().then(function(args) {
          // Should have loaded ID4 as the selected printer, since it matches
          // the rules.
          assertEquals('ID4', args.destinationId);
          assertEquals(PrinterType.LOCAL, args.type);
          assertEquals('ID4', destinationStore.selectedDestination.id);
        });
      });

  /**
   * Tests that if the system default printer policy is enabled the system
   * default printer is automatically selected even if the user has recent
   * destinations.
   */
  test(
      assert(destination_store_test.TestNames.SystemDefaultPrinterPolicy),
      function() {
        // Set the policy in loadTimeData.
        loadTimeData.overrideValues({useSystemDefaultPrinter: true});

        // Setup some recent destinations to ensure they are not selected.
        const recentDestinations = [];
        destinations.slice(0, 3).forEach(destination => {
          recentDestinations.push(makeRecentDestination(destination));
        });

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        return Promise
            .all([
              setInitialSettings(),
              eventToPromise(
                  DestinationStore.EventType
                      .SELECTED_DESTINATION_CAPABILITIES_READY,
                  destinationStore),
            ])
            .then(() => {
              // Need to load FooDevice as the printer, since it is the system
              // default.
              assertEquals(
                  'FooDevice', destinationStore.selectedDestination.id);
            });
      });

  /**
   * Tests that if there is no system default destination, the default
   * selection rules and recent destinations are empty, and the preview
   * is in app kiosk mode (so no PDF printer), the first destination returned
   * from printer fetch is selected.
   */
  test(
      assert(destination_store_test.TestNames.KioskModeSelectsFirstPrinter),
      function() {
        initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
        initialSettings.serializedAppStateStr = '';
        initialSettings.pdfPrinterDisabled = true;
        initialSettings.printerName = '';

        return setInitialSettings().then(function(args) {
          // Should have loaded the first destination as the selected printer.
          assertEquals(destinations[0].id, args.destinationId);
          assertEquals(PrinterType.LOCAL, args.type);
          assertEquals(
              destinations[0].id, destinationStore.selectedDestination.id);
        });
      });

  /**
   * Tests that if there is no system default destination, the default
   * selection rules and recent destinations are empty, the preview
   * is in app kiosk mode (so no PDF printer), and there are no
   * destinations found, the NO_DESTINATIONS error is fired and the selected
   * destination is null.
   */
  test(
      assert(destination_store_test.TestNames.NoPrintersShowsError),
      function() {
        initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
        initialSettings.serializedAppStateStr = '';
        initialSettings.pdfPrinterDisabled = true;
        initialSettings.printerName = '';
        localDestinations = [];

        return Promise
            .all([
              setInitialSettings(true),
              eventToPromise(
                  DestinationStore.EventType.ERROR, destinationStore),
            ])
            .then(function(argsArray) {
              const errorEvent = argsArray[1];
              assertEquals(
                  DestinationErrorType.NO_DESTINATIONS, errorEvent.detail);
              assertEquals(null, destinationStore.selectedDestination);
            });
      });

  /**
   * Tests that if the user has a recent destination that triggers a cloud
   * print error this does not disable the dialog.
   */
  test(
      assert(destination_store_test.TestNames.UnreachableRecentCloudPrinter),
      function() {
        const cloudPrinter = createDestinationWithCertificateStatus(
            'BarDevice', 'BarName', false);
        const recentDestination = makeRecentDestination(cloudPrinter);
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: [recentDestination],
        });
        initialSettings.userAccounts = ['foo@chromium.org'];

        return setInitialSettings().then(function(args) {
          assertEquals('FooDevice', args.destinationId);
          assertEquals(PrinterType.LOCAL, args.type);
          assertEquals('FooDevice', destinationStore.selectedDestination.id);
        });
      });

  /**
   * Tests that if the user has a recent destination that is already in the
   * store (PDF printer), the DestinationStore does not try to select a
   * printer again later. Regression test for https://crbug.com/927162.
   */
  test(assert(destination_store_test.TestNames.RecentSaveAsPdf), function() {
    const pdfPrinter = getSaveAsPdfDestination();
    const recentDestination = makeRecentDestination(pdfPrinter);
    initialSettings.serializedAppStateStr = JSON.stringify({
      version: 2,
      recentDestinations: [recentDestination],
    });

    DestinationStore.AUTO_SELECT_TIMEOUT_ = 0;
    return setInitialSettings()
        .then(function() {
          assertEquals(
              Destination.GooglePromotedId.SAVE_AS_PDF,
              destinationStore.selectedDestination.id);
          return new Promise(resolve => setTimeout(resolve));
        })
        .then(function() {
          // Should still have Save as PDF.
          assertEquals(
              Destination.GooglePromotedId.SAVE_AS_PDF,
              destinationStore.selectedDestination.id);
        });
  });

  /**
   * Tests that if there are recent destinations from different accounts, only
   * destinations associated with the most recent account are fetched.
   */
  test(
      assert(
          destination_store_test.TestNames.MultipleRecentDestinationsAccounts),
      function() {
        const account1 = 'foo@chromium.org';
        const account2 = 'bar@chromium.org';
        const driveUser1 = getGoogleDriveDestination(account1);
        const driveUser2 = getGoogleDriveDestination(account2);
        const cloudPrinterUser1 = new Destination(
            'FooCloud', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
            'FooCloudName', DestinationConnectionStatus.ONLINE,
            {account: account1});
        const recentDestinations = [
          makeRecentDestination(driveUser1),
          makeRecentDestination(driveUser2),
          makeRecentDestination(cloudPrinterUser1),
        ];
        cloudDestinations = [driveUser1, driveUser2, cloudPrinterUser1];
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });
        initialSettings.userAccounts = [account1, account2];
        initialSettings.syncAvailable = true;

        return setInitialSettings().then(() => {
          // Should have loaded Google Drive as the selected printer, since it
          // was most recent.
          assertEquals(
              Destination.GooglePromotedId.DOCS,
              destinationStore.selectedDestination.id);

          // Only the most recent printer + Save as PDF are in the store.
          const loadedPrintersAccount1 =
              destinationStore.destinations(account1);
          assertEquals(2, loadedPrintersAccount1.length);
          cloudDestinations.forEach((destination) => {
            assertEquals(
                destination === driveUser1,
                loadedPrintersAccount1.some(p => p.key == destination.key));
          });
          assertEquals(1, numPrintersSelected);

          // Only Save as PDF exists when filtering for account 2.
          const loadedPrintersAccount2 =
              destinationStore.destinations(account2);
          assertEquals(1, loadedPrintersAccount2.length);
          assertEquals(
              Destination.GooglePromotedId.SAVE_AS_PDF,
              loadedPrintersAccount2[0].id);
        });
      });

  /**
   * Tests that if the user has a single valid recent destination the
   * destination is automatically reselected.
   */
  test(
      assert(destination_store_test.TestNames.LoadAndSelectDestination),
      function() {
        destinations = getDestinations(nativeLayer, localDestinations);
        initialSettings.printerName = '';
        const id1 = 'ID1';
        const name1 = 'One';
        let destination = null;

        return setInitialSettings()
            .then(function(args) {
              assertEquals(
                  Destination.GooglePromotedId.SAVE_AS_PDF, args.destinationId);
              assertEquals(PrinterType.LOCAL, args.type);
              assertEquals(
                  Destination.GooglePromotedId.SAVE_AS_PDF,
                  destinationStore.selectedDestination.id);
              // Update destination with ID 1 so that it has policies.
              const localDestinationInfo = {
                deviceName: id1,
                printerName: name1
              };
              if (isChromeOS) {
                localDestinationInfo.policies = {
                  allowedColorModes: 0x1,  // ColorModeRestriction.MONOCHROME
                  defaultColorMode: 0x1,   // ColorModeRestriction.MONOCHROME
                };
              }
              nativeLayer.setLocalDestinationCapabilities({
                printer: localDestinationInfo,
                capabilities: getCddTemplate(id1, name1),
              });
              destinationStore.startLoadAllDestinations();
              return nativeLayer.whenCalled('getPrinters');
            })
            .then(() => {
              destination =
                  destinationStore.destinations().find(d => d.id === id1);
              // No capabilities or policies yet.
              assertFalse(!!destination.capabilities);
              if (isChromeOS) {
                assertEquals(null, destination.policies);
              }
              destinationStore.selectDestination(destination);
              return nativeLayer.whenCalled('getPrinterCapabilities');
            })
            .then(() => {
              assertEquals(destination, destinationStore.selectedDestination);
              // Capabilities are updated.
              assertTrue(!!destination.capabilities);
              if (isChromeOS) {
                // Policies are updated.
                assertTrue(!!destination.policies);
              }
            });
      });
});
