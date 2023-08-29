// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationStore, LocalDestinationInfo, makeRecentDestination, NativeLayerImpl, PrinterSetupInfoMessageType, PrintPreviewDestinationDialogCrosElement, PrintPreviewPrinterSetupInfoCrosElement, RecentDestination} from 'chrome://print/print_preview.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {NativeLayerCrosStub, setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getDestinations, setupTestListenerElement} from './print_preview_test_utils.js';

const destination_dialog_cros_test = {
  suiteName: 'DestinationDialogCrosTest',
  TestNames: {
    ShowProvisionalDialog: 'ShowProvisionalDialog',
    PrintServersChanged: 'PrintServersChanged',
    PrintServerSelected: 'PrintServerSelected',
    PrinterSetupAssistanceHasDestinations:
        'PrinterSetupAssistanceHasDestinations',
    PrinterSetupAssistanceHasNoDestinations:
        'PrinterSetupAssistanceHasNoDestinations',
    ManagePrintersMetrics_HasDestinations:
        'ManagePrintersMetrics_HasDestinations',
    ManagePrintersMetrics_HasNoDestinations:
        'ManagePrintersMetrics_HasNoDestinations',
    PrinterSetupAssistanceHasDestinations_ShowManagedPrintersFalse:
        'PrinterSetupAssistanceHasDestinations_ShowManagedPrintersFalse',
  },
};

Object.assign(
    window, {destination_dialog_cros_test: destination_dialog_cros_test});

suite(destination_dialog_cros_test.suiteName, function() {
  let dialog: PrintPreviewDestinationDialogCrosElement;

  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

  let nativeLayerCros: NativeLayerCrosStub;

  let destinations: Destination[] = [];

  const localDestinations: LocalDestinationInfo[] = [];

  let recentDestinations: RecentDestination[] = [];

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

    // Set up dialog
    dialog = document.createElement('print-preview-destination-dialog-cros');
    dialog.destinationStore = destinationStore;
  });

  function finishSetup() {
    document.body.appendChild(dialog);
    return nativeLayer.whenCalled('getPrinterCapabilities')
        .then(function() {
          destinationStore.startLoadAllDestinations();
          dialog.show();
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
      destination_dialog_cros_test.TestNames.ShowProvisionalDialog,
      async () => {
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
      destination_dialog_cros_test.TestNames.PrintServersChanged, async () => {
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
        await waitAfterNextRender(dialog);

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
      destination_dialog_cros_test.TestNames.PrintServerSelected, async () => {
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
        await waitAfterNextRender(dialog);
        nativeLayerCros.reset();

        const pendingPrintServerId =
            nativeLayerCros.whenCalled('choosePrintServers');
        dialog.shadowRoot!.querySelector('cr-searchable-drop-down')!.value =
            'Print Server 2';
        await waitAfterNextRender(dialog);

        assertEquals(1, nativeLayerCros.getCallCount('choosePrintServers'));
        assertDeepEquals(
            ['user-print-server-2', 'device-print-server-2'],
            await pendingPrintServerId);
      });

  // Test that the correct elements are displayed when the printer setup
  // assistance flag is on and destination store has destinations.
  test(
      destination_dialog_cros_test.TestNames
          .PrinterSetupAssistanceHasDestinations,
      async () => {
        // Set flag enabled.
        loadTimeData.overrideValues({
          isPrintPreviewSetupAssistanceEnabled: true,
        });
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
  // assistance flag is on and destination store has no destinations.
  test(
      destination_dialog_cros_test.TestNames
          .PrinterSetupAssistanceHasNoDestinations,
      async () => {
        // Set flag enabled.
        loadTimeData.overrideValues({
          isPrintPreviewSetupAssistanceEnabled: true,
        });
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
      destination_dialog_cros_test.TestNames
          .ManagePrintersMetrics_HasDestinations,
      async () => {
        // Set flag enabled.
        loadTimeData.overrideValues({
          isPrintPreviewSetupAssistanceEnabled: true,
        });
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
      destination_dialog_cros_test.TestNames
          .ManagePrintersMetrics_HasNoDestinations,
      async () => {
        // Set flag enabled.
        loadTimeData.overrideValues({
          isPrintPreviewSetupAssistanceEnabled: true,
        });
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
      destination_dialog_cros_test.TestNames
          .PrinterSetupAssistanceHasDestinations_ShowManagedPrintersFalse,
      async () => {
        // Set flag enabled.
        loadTimeData.overrideValues({
          isPrintPreviewSetupAssistanceEnabled: true,
        });
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
});
