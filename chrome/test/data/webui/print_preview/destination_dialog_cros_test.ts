// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Destination, DestinationStore, LocalDestinationInfo, PrintPreviewDestinationDialogCrosElement, RecentDestination} from 'chrome://print/print_preview.js';
import {DESTINATION_DIALOG_CROS_LOADING_TIMER_IN_MS, makeRecentDestination, NativeLayerImpl, PrinterSetupInfoMessageType, PrintPreviewPrinterSetupInfoCrosElement} from 'chrome://print/print_preview.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import type {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getDestinations, setupTestListenerElement} from './print_preview_test_utils.js';

suite('DestinationDialogCrosTest', function() {
  let dialog: PrintPreviewDestinationDialogCrosElement;

  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

  let nativeLayerCros: NativeLayerCrosStub;

  let destinations: Destination[] = [];

  const localDestinations: LocalDestinationInfo[] = [];

  let recentDestinations: RecentDestination[] = [];

  let mockTimer: MockTimer;

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    // Create data classes
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    nativeLayerCros = setNativeLayerCrosInstance();
    destinationStore = createDestinationStore();
    destinations = getDestinations(localDestinations);
    recentDestinations = [makeRecentDestination(destinations[4]!)];
    nativeLayer.setLocalDestinations(localDestinations);
    destinationStore.init(
        false /* pdfPrinterDisabled */, false /* saveToDriveDisabled */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        recentDestinations /* recentDestinations */);

    // Setup fake timer.
    mockTimer = new MockTimer();
    mockTimer.install();

    // Set up dialog
    dialog = document.createElement('print-preview-destination-dialog-cros');
    dialog.destinationStore = destinationStore;
  });

  teardown(function() {
    mockTimer.uninstall();
  });

  function finishSetup() {
    document.body.appendChild(dialog);
    return nativeLayer.whenCalled('getPrinterCapabilities')
        .then(function() {
          destinationStore.startLoadAllDestinations();
          dialog.show();
          mockTimer.tick(DESTINATION_DIALOG_CROS_LOADING_TIMER_IN_MS);
          return nativeLayer.whenCalled('getPrinters');
        })
        .then(function() {
          return nativeLayerCros.whenCalled('getShowManagePrinters');
        })
        .then(function() {
          flush();
        });
  }

  /**
   * Remove and recreate destination-dialog-cros then return `finishSetup`. If
   * `removeDestinations` is set, also overrides destination-store to be empty.
   */
  function recreateElementAndFinishSetup(removeDestinations: boolean):
      Promise<void> {
    // Remove existing dialog.
    dialog.remove();
    flush();

    if (removeDestinations) {
      // Re-create data classes with no destinations.
      destinationStore = createDestinationStore();
      nativeLayer.setLocalDestinations([]);
      destinationStore.init(
          false /* pdfPrinterDisabled */, false /* saveToDriveDisabled */,
          'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          [] /* recentDestinations */);
    }

    // Set up dialog.
    dialog = document.createElement('print-preview-destination-dialog-cros');
    dialog.destinationStore = destinationStore;
    return finishSetup();
  }

  /**
   * Checks that `recordInHistogram` is called with expected bucket.
   */
  function verifyRecordInHistogramCall(
      callIndex: number, expectedBucket: number): void {
    const calls = nativeLayer.getArgs('recordInHistogram');
    assertTrue(!!calls && calls.length > 0);
    assertTrue(callIndex < calls.length);
    const [histogramName, bucket] = calls[callIndex];
    assertEquals('PrintPreview.PrinterSettingsLaunchSource', histogramName);
    assertEquals(expectedBucket, bucket);
  }

  // Test that clicking a provisional destination shows the provisional
  // destinations dialog, and that the escape key closes only the provisional
  // dialog when it is open, not the destinations dialog.
  test(
      'ShowProvisionalDialog', async () => {
        const provisionalDestination = {
          extensionId: 'ABC123',
          extensionName: 'ABC Printing',
          id: 'XYZDevice',
          name: 'XYZ',
          provisional: true,
        };

        // Set the extension destinations and force the destination store to
        // reload printers.
        nativeLayer.setExtensionDestinations([[provisionalDestination]]);
        await finishSetup();
        flush();
        const provisionalDialog = dialog.shadowRoot!.querySelector(
            'print-preview-provisional-destination-resolver')!;
        assertFalse(provisionalDialog.$.dialog.open);
        const list =
            dialog.shadowRoot!.querySelector('print-preview-destination-list')!;
        const printerItems = list.shadowRoot!.querySelectorAll(
            'print-preview-destination-list-item');

        // Should have 5 local destinations, Save as PDF + extension
        // destination.
        assertEquals(7, printerItems.length);
        const provisionalItem = Array.from(printerItems).find(printerItem => {
          return printerItem.destination.id === provisionalDestination.id;
        });

        // Click the provisional destination to select it.
        provisionalItem!.click();
        flush();
        assertTrue(provisionalDialog.$.dialog.open);

        // Send escape key on provisionalDialog. Destinations dialog should
        // not close.
        const whenClosed = eventToPromise('close', provisionalDialog);
        keyEventOn(provisionalDialog, 'keydown', 19, [], 'Escape');
        flush();
        await whenClosed;

        assertFalse(provisionalDialog.$.dialog.open);
        assertTrue(dialog.$.dialog.open);
      });

  // Test that checks that print server searchable input and its selections are
  // updated according to the PRINT_SERVERS_CHANGED event.
  test(
      'PrintServersChanged', async () => {
        await finishSetup();

        const printServers = [
          {id: 'print-server-1', name: 'Print Server 1'},
          {id: 'print-server-2', name: 'Print Server 2'},
        ];
        const isSingleServerFetchingMode = true;
        webUIListenerCallback('print-servers-config-changed', {
          printServers: printServers,
          isSingleServerFetchingMode: isSingleServerFetchingMode,
        });
        flush();

        assertFalse(dialog.shadowRoot!
                        .querySelector<HTMLElement>(
                            '.server-search-box-input')!.hidden);
        const serverSelector =
            dialog.shadowRoot!.querySelector('.server-search-box-input')!;
        const serverSelections =
            serverSelector.shadowRoot!.querySelectorAll('.list-item');
        assertEquals(
            'Print Server 1', serverSelections[0]!.textContent!.trim());
        assertEquals(
            'Print Server 2', serverSelections[1]!.textContent!.trim());
      });

  // Tests that choosePrintServers is called when the print server searchable
  // input value is changed.
  test(
      'PrintServerSelected', async () => {
        await finishSetup();
        const printServers = [
          {id: 'user-print-server-1', name: 'Print Server 1'},
          {id: 'user-print-server-2', name: 'Print Server 2'},
          {id: 'device-print-server-1', name: 'Print Server 1'},
          {id: 'device-print-server-2', name: 'Print Server 2'},
        ];
        const isSingleServerFetchingMode = true;
        webUIListenerCallback('print-servers-config-changed', {
          printServers: printServers,
          isSingleServerFetchingMode: isSingleServerFetchingMode,
        });
        flush();
        nativeLayerCros.reset();

        const pendingPrintServerId =
            nativeLayerCros.whenCalled('choosePrintServers');
        dialog.shadowRoot!.querySelector('searchable-drop-down-cros')!.value =
            'Print Server 2';
        flush();

        assertEquals(1, nativeLayerCros.getCallCount('choosePrintServers'));
        assertDeepEquals(
            ['user-print-server-2', 'device-print-server-2'],
            await pendingPrintServerId);
      });

  // Test that the correct elements are displayed when the destination store has
  // destinations.
  test(
      'PrinterSetupAssistanceHasDestinations', async () => {
        await recreateElementAndFinishSetup(/*removeDestinations=*/ false);

        // Manage printers button hidden when there are valid destinations.
        const managePrintersButton =
            dialog.shadowRoot!.querySelector<HTMLElement>(
                'cr-button:not(.cancel-button)');
        assertTrue(isVisible(managePrintersButton));

        // Printer setup element should not be displayed when there are
        // valid destinations.
        const printerSetupInfo = dialog.shadowRoot!.querySelector<HTMLElement>(
            'print-preview-printer-setup-info-cros')!;
        assertTrue(printerSetupInfo.hidden);

        // Destination list should display if there are valid destinations.
        const destinationList =
            dialog.shadowRoot!.querySelector<HTMLElement>('#printList');
        assertTrue(isVisible(destinationList));

        // Destination search box should be shown if there are valid
        // destinations.
        const searchBox = dialog.shadowRoot!.querySelector<HTMLElement>(
            'print-preview-search-box');
        assertTrue(isVisible(searchBox));
      });

  // Test that the correct elements are displayed when the printer setup
  // assistance flag is on and destination store has found destinations but
  // is still searching for more.
  test('PrinterSetupAssistanceHasDestinationsSearching', async () => {
    nativeLayer.setSimulateNoResponseForGetPrinters(true);

    document.body.appendChild(dialog);
    await nativeLayer.whenCalled('getPrinterCapabilities');
    destinationStore.startLoadAllDestinations();
    dialog.show();
    flush();

    // Throbber should show while DestinationStore is still searching.
    const throbber =
        dialog.shadowRoot!.querySelector<HTMLElement>('.throbber-container');
    assertTrue(!!throbber);
    assertFalse(
        throbber.hidden,
        'Loading UI should display while DestinationStore is searching');

    // Manage printers button hidden when there are valid destinations.
    const managePrintersButton = dialog.shadowRoot!.querySelector<HTMLElement>(
        'cr-button:not(.cancel-button)');
    assertTrue(isVisible(managePrintersButton));

    // Printer setup element should not be displayed when there are
    // valid destinations.
    const printerSetupInfo = dialog.shadowRoot!.querySelector<HTMLElement>(
        'print-preview-printer-setup-info-cros')!;
    assertTrue(printerSetupInfo.hidden);

    // Destination list should display if there are valid destinations.
    const destinationList =
        dialog.shadowRoot!.querySelector<HTMLElement>('#printList');
    assertTrue(isVisible(destinationList));

    // Destination search box should be shown if there are valid
    // destinations.
    const searchBox = dialog.shadowRoot!.querySelector<HTMLElement>(
        'print-preview-search-box');
    assertTrue(isVisible(searchBox));
  });

  // Test that the correct elements are displayed when the printer setup
  // assistance flag is on and destination store has no destinations.
  test(
      'PrinterSetupAssistanceHasNoDestinations', async () => {
        await recreateElementAndFinishSetup(/*removeDestinations=*/ true);

        // Manage printers button hidden when there are no destinations.
        const managePrintersButton =
            dialog.shadowRoot!.querySelector<HTMLElement>(
                'cr-button:not(.cancel-button)');
        assertFalse(isVisible(managePrintersButton));

        // Printer setup element should be displayed when there are no valid
        // destinations.
        const printerSetupInfo =
            dialog.shadowRoot!
                .querySelector<PrintPreviewPrinterSetupInfoCrosElement>(
                    PrintPreviewPrinterSetupInfoCrosElement.is)!;
        assertFalse(printerSetupInfo.hidden);
        assertEquals(
            PrinterSetupInfoMessageType.NO_PRINTERS,
            printerSetupInfo!.messageType);

        // Destination list should be hidden if there are no valid destinations.
        const destinationList =
            dialog.shadowRoot!.querySelector<HTMLElement>('#printList');
        assertFalse(isVisible(destinationList));

        // Destination search box should be hidden if there are no valid
        // destinations.
        const searchBox = dialog.shadowRoot!.querySelector<HTMLElement>(
            'print-preview-search-box');
        assertFalse(isVisible(searchBox));
      });

  // Test that `PrintPreview.PrinterSettingsLaunchSource` metric is recorded
  // with bucket `DESTINATION_DIALOG_CROS_HAS_PRINTERS` when flag is on and
  // destination store has destinations.
  test(
      'ManagePrintersMetrics_HasDestinations', async () => {
        await recreateElementAndFinishSetup(/*removeDestinations=*/ false);

        assertEquals(0, nativeLayer.getCallCount('recordInHistogram'));

        // Manage printers button hidden when there are valid destinations.
        const managePrintersButton =
            dialog.shadowRoot!.querySelector<HTMLElement>(
                'cr-button:not(.cancel-button)')!;
        managePrintersButton.click();

        // Call should use bucket `DESTINATION_DIALOG_CROS_HAS_PRINTERS`.
        verifyRecordInHistogramCall(/*callIndex=*/ 0, /*bucket=*/ 2);
      });

  // Test that `PrintPreview.PrinterSettingsLaunchSource` metric is recorded
  // with bucket `DESTINATION_DIALOG_CROS_NO_PRINTERS` when flag is on and
  // destination store has no destinations.
  test(
      'ManagePrintersMetrics_HasNoDestinations', async () => {
        await recreateElementAndFinishSetup(/*removeDestinations=*/ true);

        assertEquals(0, nativeLayer.getCallCount('recordInHistogram'));

        // Manage printers button hidden when there are no destinations.
        const printerSetupInfo =
            dialog.shadowRoot!
                .querySelector<PrintPreviewPrinterSetupInfoCrosElement>(
                    PrintPreviewPrinterSetupInfoCrosElement.is)!;
        const managePrintersButton =
            printerSetupInfo.shadowRoot!.querySelector<HTMLElement>(
                'cr-button')!;
        managePrintersButton.click();

        // Call should use bucket `DESTINATION_DIALOG_CROS_NO_PRINTERS`.
        verifyRecordInHistogramCall(/*callIndex=*/ 0, /*bucket=*/ 1);
      });

  // Test that the correct elements are displayed when the printer setup
  // assistance flag is on, destination store has destinations, and
  // getShowManagePrinters return false. Simulates opening print preview from
  // a UI which cannot launch settings (ex. OS Settings app).
  test(
      'PrinterSetupAssistanceHasDestinations_ShowManagedPrintersFalse',
      async () => {
        nativeLayerCros.setShowManagePrinters(false);
        await recreateElementAndFinishSetup(/*removeDestinations=*/ false);

        // Manage printers button hidden when show manage printers returns
        // false.
        const managePrintersButton =
            dialog.shadowRoot!.querySelector<HTMLElement>('#managePrinters');
        assertFalse(isVisible(managePrintersButton));

        // Cancel button shown when there are valid destinations.
        const cancelButton = dialog.shadowRoot!.querySelector<HTMLElement>(
            'cr-button.cancel-button');
        assertTrue(isVisible(cancelButton));

        // Printer setup element should not be displayed when there are
        // valid destinations.
        const printerSetupInfo = dialog.shadowRoot!.querySelector<HTMLElement>(
            'print-preview-printer-setup-info-cros')!;
        assertTrue(printerSetupInfo.hidden);

        // Destination list should display if there are valid destinations.
        const destinationList =
            dialog.shadowRoot!.querySelector<HTMLElement>('#printList');
        assertTrue(isVisible(destinationList));

        // Destination search box should be shown if there are valid
        // destinations.
        const searchBox = dialog.shadowRoot!.querySelector<HTMLElement>(
            'print-preview-search-box');
        assertTrue(isVisible(searchBox));
      });

  // Test loading icon rendered while destinations are loading and for a minimum
  // of 2 seconds before destination list and search box are visible.
  test(
      'CorrectlyDisplaysAndHidesLoadingUI', async () => {
        document.body.appendChild(dialog);
        mockTimer.install();
        await nativeLayer.whenCalled('getPrinterCapabilities');
        destinationStore.startLoadAllDestinations();
        dialog.show();
        flush();

        // Dialog should be visible with loading UI displayed.
        const throbber = dialog.shadowRoot!.querySelector<HTMLElement>(
            '.throbber-container');
        assertTrue(!!throbber);
        assertFalse(
            throbber.hidden,
            'Loading UI should display while timer is running and ' +
                'destinations have not loaded');

        // Move timer forward to clear delay.
        mockTimer.tick(DESTINATION_DIALOG_CROS_LOADING_TIMER_IN_MS);

        // Dialog should be visible with loading UI displayed.
        assertFalse(
            throbber.hidden,
            'Loading UI should display while destinations have not loaded');

        // Get destinations.
        await nativeLayer.whenCalled('getPrinters');
        flush();

        // Loading UI should be hidden. Destination list and search box should
        // be visible.
        assertTrue(
            throbber.hidden,
            'Loading UI should be hidden after timer is cleared and ' +
                'destinations have loaded');
        assertTrue(
            isChildVisible(dialog, '#printList'),
            'Destination list should display');
        assertTrue(
            isChildVisible(dialog, 'print-preview-search-box'),
            'Search-box should display');
      });

  // Tests that the destination list starts hidden then will resize to display
  // destinations once they are loaded.
  test('NewDestinationsShowsAndResizesList', async () => {
    await recreateElementAndFinishSetup(/*removeDestinations=*/ true);

    // The list should start hidden and empty since there aren't destinations
    // available.
    const list =
        dialog.shadowRoot!.querySelector('print-preview-destination-list')!;
    let printerItems = list.shadowRoot!.querySelectorAll(
        'print-preview-destination-list-item');
    assertFalse(isVisible(list));
    assertEquals(0, printerItems.length);

    // Add destinations then trigger them to load.
    nativeLayer.setLocalDestinations(localDestinations);
    await destinationStore.reloadLocalPrinters();
    flush();

    // Now expect all the local destinations to be drawn.
    printerItems = list.shadowRoot!.querySelectorAll(
        'print-preview-destination-list-item');
    assertTrue(isVisible(list));
    assertEquals(6, printerItems.length);
  });
});
