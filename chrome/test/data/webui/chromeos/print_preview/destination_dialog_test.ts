// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Destination, DestinationStore, LocalDestinationInfo, PrintPreviewDestinationDialogCrosElement, PrintPreviewDestinationListItemElement} from 'chrome://print/print_preview.js';
import {DESTINATION_DIALOG_CROS_LOADING_TIMER_IN_MS, GooglePromotedDestinationId, makeRecentDestination, NativeLayerImpl} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';

import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getDestinations, getExtensionDestinations, setupTestListenerElement} from './print_preview_test_utils.js';

suite('DestinationDialogTest', function() {
  let dialog: PrintPreviewDestinationDialogCrosElement;

  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

  let destinations: Destination[] = [];

  let extensionDestinations: Destination[] = [];

  const localDestinations: LocalDestinationInfo[] = [];

  let mockTimer: MockTimer;

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    // Create data classes
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    mockTimer = new MockTimer();
    mockTimer.install();
    setNativeLayerCrosInstance();
    destinationStore = createDestinationStore();
    destinations = getDestinations(localDestinations);
    const extensionDestinationConfig = getExtensionDestinations();
    nativeLayer.setLocalDestinations(localDestinations);
    nativeLayer.setExtensionDestinations(extensionDestinationConfig.infoLists);
    extensionDestinations = extensionDestinationConfig.destinations;
  });

  function finishSetup() {
    // Set up dialog
    dialog = document.createElement('print-preview-destination-dialog-cros');
    dialog.destinationStore = destinationStore;
    document.body.appendChild(dialog);
    destinationStore.startLoadAllDestinations();
    dialog.show();
    mockTimer.tick(DESTINATION_DIALOG_CROS_LOADING_TIMER_IN_MS);
  }

  function validatePrinterList() {
    const list =
        dialog.shadowRoot!.querySelector('print-preview-destination-list');
    const printerItems = list!.shadowRoot!.querySelectorAll(
        'print-preview-destination-list-item');
    const getDisplayedName = (item: PrintPreviewDestinationListItemElement) =>
        item.shadowRoot!.querySelector('.name')!.textContent;
    // 5 local printers + 3 extension printers + Save as PDF
    assertEquals(9, printerItems.length);
    // Save as PDF shows up first.
    assertEquals(
        GooglePromotedDestinationId.SAVE_AS_PDF,
        getDisplayedName(printerItems[0]!));
    Array.from(printerItems).slice(1, 5).forEach((item, index) => {
      assertEquals(destinations[index]!.displayName, getDisplayedName(item));
    });
    assertEquals('FooName', getDisplayedName(printerItems[5]!));
    Array.from(printerItems).slice(6, 9).forEach((item, index) => {
      assertEquals(
          extensionDestinations[index]!.displayName, getDisplayedName(item));
    });
  }

  // Test that destinations are correctly displayed in the lists.
  test('PrinterList', async () => {
    // Native printers are fetched at startup, since the recent printer is set
    // as native.
    let whenPrinterListReady = nativeLayer.waitForGetPrinters(1);
    destinationStore.init(
        false /* pdfPrinterDisabled */, false /* saveToDriveDisabled */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        [makeRecentDestination(destinations[4]!)] /* recentDestinations */);
    await whenPrinterListReady;
    whenPrinterListReady = nativeLayer.waitForGetPrinters(1);
    // This should trigger 1 new getPrinters() call, for extension printers.
    finishSetup();
    await whenPrinterListReady;
    flush();
    validatePrinterList();
  });

  // Test that destinations are correctly displayed in the lists when all
  // printers have been preloaded before the dialog is opened. Regression test
  // for https://crbug.com/1330678.
  test(
      'PrinterListPreloaded', async () => {
        // All printers are fetched at startup since both native and extension
        // printers are recent.
        const whenAllPreloaded = nativeLayer.waitForGetPrinters(2);
        destinationStore.init(
            false /* pdfPrinterDisabled */, false /* saveToDriveDisabled */,
            'FooDevice' /* printerName */,
            '' /* serializedDefaultDestinationSelectionRulesStr */, [
              makeRecentDestination(destinations[4]!),
              makeRecentDestination(extensionDestinations[0]!),
            ] /* recentDestinations */);
        await whenAllPreloaded;
        finishSetup();
        flush();
        validatePrinterList();
      });
});
