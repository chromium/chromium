// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DestinationStore, LocalDestinationInfo, NativeInitialSettings} from 'chrome://print/print_preview.js';
import {Destination, DestinationErrorType, DestinationOrigin, DestinationStoreEventType, GooglePromotedDestinationId, makeRecentDestination, NativeLayerImpl,
        // <if expr="is_chromeos">
        PrinterStatusReason, PrinterStatusSeverity,
        // </if>
        PrinterType} from 'chrome://print/print_preview.js';
// <if expr="not is_chromeos">
import type {RecentDestination} from 'chrome://print/print_preview.js';
// </if>

// <if expr="is_chromeos">
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
// </if>
// <if expr="not is_chromeos">
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
// </if>
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// <if expr="is_chromeos">
import type {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getCddTemplate, getDefaultInitialSettings, getDestinations, getSaveAsPdfDestination, setupTestListenerElement} from './print_preview_test_utils.js';

suite('DestinationStoreTest', function() {
  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

  // <if expr="is_chromeos">
  let nativeLayerCros: NativeLayerCrosStub;
  // </if>

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
    nativeLayerCros = setNativeLayerCrosInstance();
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
    const saveToDriveDisabled =
        initialSettings.pdfPrinterDisabled || !initialSettings.isDriveMounted;
    destinationStore.init(
        initialSettings.pdfPrinterDisabled, saveToDriveDisabled,
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
      'SingleRecentDestination', function() {
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
      'MultipleRecentDestinations', function() {
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
      'RecentDestinationsFallback', function() {
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
      'MultipleRecentDestinationsOneRequest', function() {
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
      'DefaultDestinationSelectionRules', function() {
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
      'SystemDefaultPrinterPolicy', function() {
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
      'KioskModeSelectsFirstPrinter', function() {
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
      'NoPrintersShowsError', function() {
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
  test('RecentSaveAsPdf', function() {
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
      'LoadAndSelectDestination', function() {
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

  /**
   * Tests that the destination store will not allow the selected destination
   * to be selected again unless explicitly requested.
   */
  test('DestinationAlreadySelected', function() {
    return setInitialSettings(false).then(() => {
      // The default destination is initialized which triggers the first
      // selection.
      assertEquals(1, numPrintersSelected);

      // Selecting a new destination will trigger another selection.
      const printer1 =
          new Destination('Printer1', DestinationOrigin.LOCAL, 'Printer1');
      destinationStore.selectDestination(printer1);
      assertEquals(2, numPrintersSelected);

      // Selecting that same destination again won't trigger another
      // selection.
      destinationStore.selectDestination(printer1);
      assertEquals(2, numPrintersSelected);

      // <if expr="is_chromeos">
      // Selecting that same destination on CrOS with the `refreshDestination`
      // parameter triggers a selection.
      destinationStore.selectDestination(
          printer1, /*refreshDestination=*/ true);
      assertEquals(3, numPrintersSelected);
      // </if>
    });
  });

  // <if expr="is_chromeos">
  /** Tests that the SAVE_TO_DRIVE_CROS destination is loaded on Chrome OS. */
  test(
      'LoadSaveToDriveCros', function() {
        return setInitialSettings(false).then(() => {
          assertTrue(!!destinationStore.destinations().find(
              destination => destination.id ===
                  GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS));
        });
      });

  // Tests that the SAVE_TO_DRIVE_CROS destination is not loaded on Chrome OS
  // when saving to Google Drive is disabled.
  test('SaveToDriveDisabled', function() {
    initialSettings.isDriveMounted =
        false;  // This setting disables saving to Google Drive.
    return setInitialSettings(false).then(() => {
      assertFalse(!!destinationStore.destinations().find(
          destination => destination.id ===
              GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS));
    });
  });

  // Tests that the destination store subscribes to the LocalPrintersObserver
  // upon initialization after a successful destination search.
  test('ObserveLocalPrintersAfterSuccessfulSearch', function() {
    const printer1 = {
      printerName: 'localPrinter1',
      deviceName: 'localPrinter1',
    };
    const printer2 = {
      printerName: 'localPrinter2',
      deviceName: 'localPrinter2',
    };
    nativeLayerCros.setLocalPrinters([printer1, printer2]);

    return setInitialSettings(/*expectPrinterFailure=*/ false).then(() => {
      assertEquals(1, nativeLayerCros.getCallCount('observeLocalPrinters'));
      assertTrue(!!destinationStore.destinations().find(
          destination => destination.id === printer1.printerName));
      assertTrue(!!destinationStore.destinations().find(
          destination => destination.id === printer2.printerName));
    });
  });

  // Tests that the destination store subscribes to the LocalPrintersObserver
  // upon initialization after no destination search is started.
  test('ObserveLocalPrintersAfterNoSearch', function() {
    const printer1 = {
      printerName: 'localPrinter1',
      deviceName: 'localPrinter1',
    };
    const printer2 = {
      printerName: 'localPrinter2',
      deviceName: 'localPrinter2',
    };
    nativeLayerCros.setLocalPrinters([printer1, printer2]);

    // Set to empty string so `systemDefaultDestinationId` destination store
    // param is empty which triggers no destination search.
    initialSettings.printerName = '';
    return setInitialSettings(/*expectPrinterFailure=*/ false).then(() => {
      assertEquals(1, nativeLayerCros.getCallCount('observeLocalPrinters'));
      assertTrue(!!destinationStore.destinations().find(
          destination => destination.id === printer1.printerName));
      assertTrue(!!destinationStore.destinations().find(
          destination => destination.id === printer2.printerName));
    });
  });

  // Tests that the destination store adds printers from the
  // 'local-printers-updated' event.
  test('LocalPrintersUpdatedEventPrintersAdded', function() {
    const printer1 = {
      printerName: 'localPrinter1',
      deviceName: 'localPrinter1',
    };
    const printer2 = {
      printerName: 'localPrinter2',
      deviceName: 'localPrinter2',
    };

    return setInitialSettings(/*expectPrinterFailure=*/ false).then(() => {
      // Confirm the printers are not in the destination store before the event
      // fires.
      assertFalse(!!destinationStore.destinations().find(
          destination => destination.id === printer1.printerName));
      assertFalse(!!destinationStore.destinations().find(
          destination => destination.id === printer2.printerName));

      // Fire the event and expect the destination store to add the local
      // printers.
      webUIListenerCallback('local-printers-updated', [printer1, printer2]);
      assertTrue(!!destinationStore.destinations().find(
          destination => destination.id === printer1.printerName));
      assertTrue(!!destinationStore.destinations().find(
          destination => destination.id === printer2.printerName));
    });
  });

  // Tests that the destination store updates printer statuses from the
  // 'local-printers-updated' event.
  test('LocalPrintersUpdatedEventStatusUpdate', function() {
    const printer1 = {
      printerName: 'localPrinter1',
      deviceName: 'localPrinter1',
      printerStatus: {},
    };
    const expectedPrinterStatus = {
      printerId: 'localPrinter1',
      statusReasons: [{
        reason: PrinterStatusReason.OUT_OF_INK,
        severity: PrinterStatusSeverity.WARNING,
      }],
    };

    return setInitialSettings(/*expectPrinterFailure=*/ false)
        .then(() => {
          // Fire the event and expect the destination to not have a printer
          // status.
          webUIListenerCallback('local-printers-updated', [printer1]);
          const destination = destinationStore.destinations().find(
              destination => destination.id === printer1.printerName);
          assertTrue(!!destination);
          assertEquals(null, destination.printerStatusReason);

          // Add a printer status then trigger the event again. The printer
          // status should be parsed and appended to the existing destination.
          printer1.printerStatus = expectedPrinterStatus;
          const onPrinterStatusUpdatePromise = eventToPromise(
              DestinationStoreEventType.DESTINATION_PRINTER_STATUS_UPDATE,
              destinationStore);
          webUIListenerCallback('local-printers-updated', [printer1]);
          return onPrinterStatusUpdatePromise;
        })
        .then(() => {
          const destination = destinationStore.destinations().find(
              destination => destination.id === printer1.printerName);
          assertTrue(!!destination);
          assertEquals(
              expectedPrinterStatus.statusReasons[0]!.reason,
              destination.printerStatusReason);
        });
  });

  // Tests that the destination store dispatches the correct event attributes
  // when the printer's online status changes.
  test('PrinterStatusOnlineChange', function() {
    const printer1 = {
      printerName: 'localPrinter1',
      deviceName: 'localPrinter1',
      printerStatus: {},
    };

    return setInitialSettings(/*expectPrinterFailure=*/ false)
        .then(() => {
          // Add an unreachable status then trigger the event to set the
          // printer's initial status.
          printer1.printerStatus = {
            printerId: 'localPrinter1',
            statusReasons: [{
              reason: PrinterStatusReason.PRINTER_UNREACHABLE,
              severity: PrinterStatusSeverity.ERROR,
            }],
          };

          const onPrinterStatusUpdatePromise = eventToPromise(
              DestinationStoreEventType.DESTINATION_PRINTER_STATUS_UPDATE,
              destinationStore);
          webUIListenerCallback('local-printers-updated', [printer1]);
          return onPrinterStatusUpdatePromise;
        })
        .then((e) => {
          assertFalse(e.detail.nowOnline);

          // Update the printer to an online error status and expect the online
          // variable to be true in the event.
          printer1.printerStatus = {
            printerId: 'localPrinter1',
            statusReasons: [{
              reason: PrinterStatusReason.PAPER_JAM,
              severity: PrinterStatusSeverity.ERROR,
            }],
          };

          const onPrinterStatusUpdatePromise = eventToPromise(
              DestinationStoreEventType.DESTINATION_PRINTER_STATUS_UPDATE,
              destinationStore);
          webUIListenerCallback('local-printers-updated', [printer1]);
          return onPrinterStatusUpdatePromise;
        })
        .then((e) => {
          assertTrue(e.detail.nowOnline);

          // Update the printer to a different online error status and expect
          // the online to be false now since the printer was already online.
          printer1.printerStatus = {
            printerId: 'localPrinter1',
            statusReasons: [{
              reason: PrinterStatusReason.LOW_ON_INK,
              severity: PrinterStatusSeverity.ERROR,
            }],
          };

          const onPrinterStatusUpdatePromise = eventToPromise(
              DestinationStoreEventType.DESTINATION_PRINTER_STATUS_UPDATE,
              destinationStore);
          webUIListenerCallback('local-printers-updated', [printer1]);
          return onPrinterStatusUpdatePromise;
        })
        .then((e) => {
          assertFalse(e.detail.nowOnline);
        });
  });
  // </if>
});
