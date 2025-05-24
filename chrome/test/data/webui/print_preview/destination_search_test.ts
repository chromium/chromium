// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {DestinationStore, PrintPreviewDestinationDialogElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, DestinationStoreEventType, NativeLayerImpl} from 'chrome://print/print_preview.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
        false /* pdfPrinterDisabled */, 'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        [] /* recentDestinations */);

    dialog = document.createElement('print-preview-destination-dialog');
    dialog.destinationStore = destinationStore;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(dialog);
    return nativeLayer.whenCalled('getPrinterCapabilities').then(function() {
      dialog.show();
      nativeLayer.reset();
    });
  });

  function simulateDestinationSelect(destId: string) {
    const destination =
        new Destination(destId, DestinationOrigin.LOCAL, 'displayName');

    // Get print list and fire event.
    const list =
        dialog.shadowRoot.querySelector('print-preview-destination-list');
    assertTrue(!!list);
    list.dispatchEvent(new CustomEvent(
        'destination-selected',
        {bubbles: true, composed: true, detail: destination}));
  }

  // Tests that a destination is selected if the user clicks on it and
  // capabilities fetch succeeds.
  test('GetCapabilitiesSucceeds', async function() {
    const destId = '00112233DEADBEEF';
    nativeLayer.setLocalDestinationCapabilities(getCddTemplate(destId));

    const waiter = eventToPromise(
        DestinationStoreEventType.DESTINATION_SELECT, destinationStore);
    simulateDestinationSelect(destId);
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
    simulateDestinationSelect(destId);
    const args = await nativeLayer.whenCalled('getPrinterCapabilities');
    assertEquals(destId, args.destinationId);
    // The destination is selected even though capabilities cannot be
    // retrieved.
    assertEquals(destId, destinationStore.selectedDestination!.id);
  });
});
