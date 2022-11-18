// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationStore, LocalDestinationInfo, makeRecentDestination, NativeLayerImpl, PrintPreviewDestinationDialogCrosElement, RecentDestination} from 'chrome://print/print_preview.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {NativeLayerCrosStub, setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getDestinations, setupTestListenerElement} from './print_preview_test_utils.js';

const destination_dialog_cros_test = {
  suiteName: 'DestinationDialogCrosTest',
  TestNames: {
    ShowProvisionalDialog: 'ShowProvisionalDialog',
    PrintServersChanged: 'PrintServersChanged',
    PrintServerSelected: 'PrintServerSelected',
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
        false /* pdfPrinterDisabled */, true /* isDriveMounted */,
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
          flush();
        });
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
});
