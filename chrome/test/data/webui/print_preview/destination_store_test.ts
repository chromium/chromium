// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationErrorType, DestinationStore, DestinationStoreEventType, GooglePromotedDestinationId, LocalDestinationInfo, makeRecentDestination, NativeInitialSettings, NativeLayerImpl, PrinterType} from 'chrome://print/print_preview.js';
// <if expr="not is_chromeos">
import {RecentDestination} from 'chrome://print/print_preview.js';
// </if>
// <if expr="not is_chromeos">
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
// </if>

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// <if expr="is_chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getCddTemplate, getDefaultInitialSettings, getDestinations, getSaveAsPdfDestination, setupTestListenerElement} from './print_preview_test_utils.js';

const destination_store_test = {
  suiteName: 'DestinationStoreTest',
  TestNames: {
    SingleRecentDestination: 'single recent destination',
    MultipleRecentDestinations: 'multiple recent destinations',
    RecentDestinationsFallback:
        'no local or other destinations results in save as pdf',
    MultipleRecentDestinationsOneRequest:
        'multiple recent destinations one request',
    DefaultDestinationSelectionRules: 'default destination selection rules',
    // <if expr="not is_chromeos">
    SystemDefaultPrinterPolicy: 'system default printer policy',
    // </if>
    KioskModeSelectsFirstPrinter: 'kiosk mode selects first printer',
    NoPrintersShowsError: 'no printers shows error',
    RecentSaveAsPdf: 'recent save as pdf',
    LoadAndSelectDestination: 'select loaded destination',
    // <if expr="is_chromeos">
    LoadSaveToDriveCros: 'load Save to Drive Cros',
    DriveNotMounted: 'drive not mounted',
    // </if>
  },
};

Object.assign(window, {destination_store_test: destination_store_test});

suite(destination_store_test.suiteName, function() {
  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

  let initialSettings: NativeInitialSettings;

  let localDestinations: LocalDestinationInfo[] = [];

  let destinations: Destination[] = [];

  let numPrintersSelected: number = 0;

  setup(function() {
    // Clear the UI.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    setupTestListenerElement();

    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="is_chromeos">
    setNativeLayerCrosInstance();
    // </if>

    initialSettings = getDefaultInitialSettings();
    localDestinations = [];
    destinations = getDestinations(localDestinations);
  });

  /*
   * Sets the initial settings to the stored value and creates the page.
   * @param expectPrinterFailure Whether printer fetch is
   *     expected to fail
   * @return Promise that resolves when initial settings and,
   *     if printer failure is not expected, printer capabilities have
   *     been returned.
   */
  function setInitialSettings(expectPrinterFailure?: boolean):
      Promise<{destinationId: string, printerType: PrinterType}> {
    // Set local print list.
    nativeLayer.setLocalDestinations(localDestinations);

    // Create destination store.
    destinationStore = createDestinationStore();

    destinationStore.addEventListener(
        DestinationStoreEventType.DESTINATION_SELECT, function() {
          numPrintersSelected++;
        });

    // Initialize.
    const recentDestinations = initialSettings.serializedAppStateStr ?
        JSON.parse(initialSettings.serializedAppStateStr).recentDestinations :
        [];
    const whenCapabilitiesReady = eventToPromise(
        DestinationStoreEventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        destinationStore);
    destinationStore.init(
        initialSettings.pdfPrinterDisabled, !!initialSettings.isDriveMounted,
        initialSettings.printerName,
        initialSettings.serializedDefaultDestinationSelectionRulesStr,
        recentDestinations);
    return expectPrinterFailure ? Promise.resolve() : Promise.race([
      nativeLayer.whenCalled('getPrinterCapabilities'),
      whenCapabilitiesReady,
    ]);
  }

  /**
   * Tests that if the user has a single valid recent destination the
   * destination is automatically reselected.
   */
  test(
      destination_store_test.TestNames.SingleRecentDestination, function() {
        const recentDestination = makeRecentDestination(destinations[0]!);
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: [recentDestination],
        });

        return setInitialSettings(false).then(args => {
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('ID1', destinationStore.selectedDestination!.id);
        });
      });

  /**
   * Tests that if the user has multiple valid recent destinations the most
   * recent destination is automatically reselected and its capabilities are
   * fetched.
   */
  test(
      destination_store_test.TestNames.MultipleRecentDestinations, function() {
        const recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        return setInitialSettings(false).then(function(args) {
          // Should have loaded ID1 as the selected printer, since it was most
          // recent.
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('ID1', destinationStore.selectedDestination!.id);
          // Verify that all local printers have been added to the store.
          const reportedPrinters = destinationStore.destinations();
          destinations.forEach(destination => {
            const match = reportedPrinters.find((reportedPrinter) => {
              return reportedPrinter.id === destination.id;
            });
            assertFalse(typeof match === 'undefined');
          });
        });
      });

  /**
   * Tests that if there are no recent destinations, we fall back to Save As
   * PDF.
   */
  test(
      destination_store_test.TestNames.RecentDestinationsFallback, function() {
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: [],
        });
        localDestinations = [];

        return setInitialSettings(false).then(() => {
          assertEquals(
              GooglePromotedDestinationId.SAVE_AS_PDF,
              destinationStore.selectedDestination!.id);
        });
      });

  /**
   * Tests that if the user has multiple valid recent destinations, the
   * correct destination is selected for the preview request.
   * For crbug.com/666595.
   */
  test(
      destination_store_test.TestNames.MultipleRecentDestinationsOneRequest,
      function() {
        const recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        return setInitialSettings(false).then(function(args) {
          // Should have loaded ID1 as the selected printer, since it was most
          // recent.
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('ID1', destinationStore.selectedDestination!.id);

          // The other local destinations should be in the store, but only one
          // should have been selected so there was only one preview request.
          const reportedPrinters = destinationStore.destinations();
          const expectedPrinters =
              // <if expr="is_chromeos">
              7;
          // </if>
          // <if expr="not is_chromeos">
          6;
          // </if>
          assertEquals(expectedPrinters, reportedPrinters.length);
          destinations.forEach(destination => {
            assertTrue(reportedPrinters.some(p => p.id === destination.id));
          });
          assertEquals(1, numPrintersSelected);
        });
      });

  /**
   * Tests that if there are default destination selection rules they are
   * respected and a matching destination is automatically selected.
   */
  test(
      destination_store_test.TestNames.DefaultDestinationSelectionRules,
      function() {
        initialSettings.serializedDefaultDestinationSelectionRulesStr =
            JSON.stringify({namePattern: '.*Four.*'});
        initialSettings.serializedAppStateStr = '';
        return setInitialSettings(false).then(function(args) {
          // Should have loaded ID4 as the selected printer, since it matches
          // the rules.
          assertEquals('ID4', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('ID4', destinationStore.selectedDestination!.id);
        });
      });

  // <if expr="not is_chromeos">
  /**
   * Tests that if the system default printer policy is enabled the system
   * default printer is automatically selected even if the user has recent
   * destinations.
   */
  test(
      destination_store_test.TestNames.SystemDefaultPrinterPolicy, function() {
        // Set the policy in loadTimeData.
        loadTimeData.overrideValues({useSystemDefaultPrinter: true});

        // Setup some recent destinations to ensure they are not selected.
        const recentDestinations: RecentDestination[] = [];
        destinations.slice(0, 3).forEach(destination => {
          recentDestinations.push(makeRecentDestination(destination));
        });

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        return Promise
            .all([
              setInitialSettings(false),
              eventToPromise(
                  DestinationStoreEventType
                      .SELECTED_DESTINATION_CAPABILITIES_READY,
                  destinationStore),
            ])
            .then(() => {
              // Need to load FooDevice as the printer, since it is the system
              // default.
              assertEquals(
                  'FooDevice', destinationStore.selectedDestination!.id);
            });
      });
  // </if>

  /**
   * Tests that if there is no system default destination, the default
   * selection rules and recent destinations are empty, and the preview
   * is in app kiosk mode (so no PDF printer), the first destination returned
   * from printer fetch is selected.
   */
  test(
      destination_store_test.TestNames.KioskModeSelectsFirstPrinter,
      function() {
        initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
        initialSettings.serializedAppStateStr = '';
        initialSettings.pdfPrinterDisabled = true;
        initialSettings.isDriveMounted = false;
        initialSettings.printerName = '';

        return setInitialSettings(false).then(function(args) {
          // Should have loaded the first destination as the selected printer.
          assertEquals(destinations[0]!.id, args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals(
              destinations[0]!.id, destinationStore.selectedDestination!.id);
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
      destination_store_test.TestNames.NoPrintersShowsError, function() {
        initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
        initialSettings.serializedAppStateStr = '';
        initialSettings.pdfPrinterDisabled = true;
        initialSettings.isDriveMounted = false;
        initialSettings.printerName = '';
        localDestinations = [];

        return Promise
            .all([
              setInitialSettings(true),
              eventToPromise(DestinationStoreEventType.ERROR, destinationStore),
            ])
            .then(function(argsArray) {
              const errorEvent = argsArray[1];
              assertEquals(
                  DestinationErrorType.NO_DESTINATIONS, errorEvent.detail);
              assertEquals(null, destinationStore.selectedDestination);
            });
      });

  /**
   * Tests that if the user has a recent destination that is already in the
   * store (PDF printer), the DestinationStore does not try to select a
   * printer again later. Regression test for https://crbug.com/927162.
   */
  test(destination_store_test.TestNames.RecentSaveAsPdf, function() {
    const pdfPrinter = getSaveAsPdfDestination();
    const recentDestination = makeRecentDestination(pdfPrinter);
    initialSettings.serializedAppStateStr = JSON.stringify({
      version: 2,
      recentDestinations: [recentDestination],
    });

    return setInitialSettings(false)
        .then(function() {
          assertEquals(
              GooglePromotedDestinationId.SAVE_AS_PDF,
              destinationStore.selectedDestination!.id);
          return new Promise(resolve => setTimeout(resolve));
        })
        .then(function() {
          // Should still have Save as PDF.
          assertEquals(
              GooglePromotedDestinationId.SAVE_AS_PDF,
              destinationStore.selectedDestination!.id);
        });
  });

  /**
   * Tests that if the user has a single valid recent destination the
   * destination is automatically reselected.
   */
  test(
      destination_store_test.TestNames.LoadAndSelectDestination, function() {
        destinations = getDestinations(localDestinations);
        initialSettings.printerName = '';
        const id1 = 'ID1';
        const name1 = 'One';
        let destination: Destination;

        return setInitialSettings(false)
            .then(function(args) {
              assertEquals(
                  GooglePromotedDestinationId.SAVE_AS_PDF, args.destinationId);
              assertEquals(PrinterType.PDF_PRINTER, args.printerType);
              assertEquals(
                  GooglePromotedDestinationId.SAVE_AS_PDF,
                  destinationStore.selectedDestination!.id);
              const localDestinationInfo = {
                deviceName: id1,
                printerName: name1,
              };
              // Typecast localDestinationInfo to work around the fact that
              // policy types are only defined on Chrome OS.
              nativeLayer.setLocalDestinationCapabilities({
                printer: localDestinationInfo,
                capabilities: getCddTemplate(id1, name1).capabilities,
              });
              destinationStore.startLoadAllDestinations();
              return nativeLayer.whenCalled('getPrinters');
            })
            .then(() => {
              destination =
                  destinationStore.destinations().find(d => d.id === id1)!;
              // No capabilities or policies yet.
              assertFalse(!!destination.capabilities);
              destinationStore.selectDestination(destination);
              return nativeLayer.whenCalled('getPrinterCapabilities');
            })
            .then(() => {
              assertEquals(destination, destinationStore.selectedDestination);
              // Capabilities are updated.
              assertTrue(!!destination.capabilities);
            });
      });

  // <if expr="is_chromeos">
  /** Tests that the SAVE_TO_DRIVE_CROS destination is loaded on Chrome OS. */
  test(
      destination_store_test.TestNames.LoadSaveToDriveCros, function() {
        return setInitialSettings(false).then(() => {
          assertTrue(!!destinationStore.destinations().find(
              destination => destination.id ===
                  GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS));
        });
      });

  // Tests that the SAVE_TO_DRIVE_CROS destination is not loaded on Chrome OS
  // when Google Drive is not mounted.
  test(destination_store_test.TestNames.DriveNotMounted, function() {
    initialSettings.isDriveMounted = false;
    return setInitialSettings(false).then(() => {
      assertFalse(!!destinationStore.destinations().find(
          destination => destination.id ===
              GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS));
    });
  });
  // </if>
});
