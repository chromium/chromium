// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DestinationStore, PrintPreviewDestinationDialogCrosElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, DestinationStoreEventType, NativeLayerCrosImpl, NativeLayerImpl} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getCddTemplate, setupTestListenerElement} from './print_preview_test_utils.js';

suite('DestinationSearchTest', function() {
  let dialog: PrintPreviewDestinationDialogCrosElement;

  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

  let nativeLayerCros: NativeLayerCrosStub;

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    // Create data classes
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    nativeLayerCros = new NativeLayerCrosStub();
    NativeLayerCrosImpl.setInstance(nativeLayerCros);
    destinationStore = createDestinationStore();
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplate('FooDevice', 'FooName'));
    destinationStore.init(
        false /* pdfPrinterDisabled */, false /* saveToDriveDisabled */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        [] /* recentDestinations */);

    // Set up dialog
    dialog = document.createElement('print-preview-destination-dialog-cros');
    dialog.destinationStore = destinationStore;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(dialog);
    return nativeLayer.whenCalled('getPrinterCapabilities').then(function() {
      dialog.show();
      flush();
      nativeLayer.reset();
    });
  });

  /** @param destination The destination to simulate selection of. */
  function simulateDestinationSelect(destination: Destination) {
    // Fake destinationListItem.
    const item = document.createElement('print-preview-destination-list-item');
    item.destination = destination;

    // Get print list and fire event.
    const list =
        dialog.shadowRoot!.querySelector('print-preview-destination-list')!;
    list.dispatchEvent(new CustomEvent(
        'destination-selected', {bubbles: true, composed: true, detail: item}));
  }

  /**
   * Adds a destination to the dialog and simulates selection of the
   * destination.
   * @param destId The ID for the destination.
   */
  function requestSetup(destId: string) {
    const dest = new Destination(destId, DestinationOrigin.CROS, 'displayName');

    // Add the destination to the list.
    simulateDestinationSelect(dest);
  }

  // Tests that a destination is selected if the user clicks on it and setup
  // succeeds.
  test(
      'ReceiveSuccessfulSetup', async function() {
        const destId = '00112233DEADBEEF';
        const response = {
          printerId: destId,
          capabilities: getCddTemplate(destId).capabilities!,
        };
        nativeLayerCros.setSetupPrinterResponse(response);

        const waiter = eventToPromise(
            DestinationStoreEventType.DESTINATION_SELECT, destinationStore);
        requestSetup(destId);
        const results = await Promise.all(
            [nativeLayerCros.whenCalled('setupPrinter'), waiter]);
        const actualId = results[0];
        assertEquals(destId, actualId);
        // After setup or capabilities fetch succeeds, the destination
        // should be selected.
        assertNotEquals(null, destinationStore.selectedDestination);
        assertEquals(destId, destinationStore.selectedDestination!.id);
      });

  // Test what happens when the setupPrinter request is rejected.
  test('ResolutionFails', async function() {
    const destId = '001122DEADBEEF';
    const originalDestination = destinationStore.selectedDestination;
    nativeLayerCros.setSetupPrinterResponse(
        {printerId: destId, capabilities: {printer: {}, version: '1'}}, true);
    requestSetup(destId);
    const actualId = await nativeLayerCros.whenCalled('setupPrinter');
    assertEquals(destId, actualId);
    // The selected printer should not have changed, since a printer
    // cannot be selected until setup succeeds.
    assertEquals(originalDestination, destinationStore.selectedDestination);
  });
});
