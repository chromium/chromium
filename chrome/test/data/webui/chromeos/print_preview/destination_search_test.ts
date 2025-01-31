// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DestinationStore, PrintPreviewDestinationDialogElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, DestinationStoreEventType, NativeLayerImpl} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getCddTemplate, setupTestListenerElement} from './print_preview_test_utils.js';

suite('DestinationSearchTest', function() {
  let dialog: PrintPreviewDestinationDialogElement;

  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    // Create data classes
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    destinationStore = createDestinationStore();
    nativeLayer.setLocalDestinationCapabilities(
        getCddTemplate('FooDevice', 'FooName'));
    destinationStore.init(
        false /* pdfPrinterDisabled */, true /* saveToDriveDisabled */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        [] /* recentDestinations */);

    dialog = document.createElement('print-preview-destination-dialog');
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
    const dest =
        new Destination(destId, DestinationOrigin.LOCAL, 'displayName');

    // Add the destination to the list.
    simulateDestinationSelect(dest);
  }

  // Tests that a destination is selected if the user clicks on it and
  // capabilities fetch succeeds.
  test(
      'GetCapabilitiesSucceeds', async function() {
        const destId = '00112233DEADBEEF';
        nativeLayer.setLocalDestinationCapabilities(getCddTemplate(destId));

        const waiter = eventToPromise(
            DestinationStoreEventType.DESTINATION_SELECT, destinationStore);
        requestSetup(destId);
        const results = await Promise.all(
            [nativeLayer.whenCalled('getPrinterCapabilities'), waiter]);
        const actualId = results[0].destinationId;
        assertEquals(destId, actualId);
        // After setup or capabilities fetch succeeds, the destination
        // should be selected.
        assertNotEquals(null, destinationStore.selectedDestination);
        assertEquals(destId, destinationStore.selectedDestination!.id);
      });

  // Tests what happens when capabilities cannot be retrieved for the chosen
  // destination. The destination will still be selected in this case.
  test('GetCapabilitiesFails', async function() {
    const destId = '001122DEADBEEF';
    nativeLayer.setLocalDestinationCapabilities(getCddTemplate(destId), true);
    requestSetup(destId);
    const args = await nativeLayer.whenCalled('getPrinterCapabilities');
    assertEquals(destId, args.destinationId);
    // The destination is selected even though capabilities cannot be
    // retrieved.
    assertEquals(destId, destinationStore.selectedDestination!.id);
  });
});
