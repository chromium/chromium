// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationStore, GooglePromotedDestinationId, LocalDestinationInfo, makeRecentDestination, NativeLayerImpl, PrintPreviewDestinationDialogElement, PrintPreviewDestinationListItemElement, RecentDestination} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getDestinations, setupTestListenerElement} from './print_preview_test_utils.js';

const destination_dialog_test = {
  suiteName: 'DestinationDialogTest',
  TestNames: {
    PrinterList: 'PrinterList',
    ShowProvisionalDialog: 'ShowProvisionalDialog',
    UserAccounts: 'UserAccounts',
  },
};

Object.assign(window, {destination_dialog_test: destination_dialog_test});

suite(destination_dialog_test.suiteName, function() {
  let dialog: PrintPreviewDestinationDialogElement;

  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

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
    dialog = document.createElement('print-preview-destination-dialog');
    dialog.destinationStore = destinationStore;
  });

  function finishSetup(): Promise<void> {
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

  // Test that destinations are correctly displayed in the lists.
  test(assert(destination_dialog_test.TestNames.PrinterList), async () => {
    await finishSetup();
    const list =
        dialog.shadowRoot!.querySelector('print-preview-destination-list');

    const printerItems = list!.shadowRoot!.querySelectorAll(
        'print-preview-destination-list-item');

    const getDisplayedName = (item: PrintPreviewDestinationListItemElement) =>
        item.shadowRoot!.querySelector('.name')!.textContent;
    // 5 printers + Save as PDF
    assertEquals(6, printerItems.length);
    // Save as PDF shows up first.
    assertEquals(
        GooglePromotedDestinationId.SAVE_AS_PDF,
        getDisplayedName(printerItems[0]!));
    assertEquals(
        'rgb(32, 33, 36)',
        window
            .getComputedStyle(
                printerItems[0]!.shadowRoot!.querySelector('.name')!)
            .color);
    Array.from(printerItems).slice(1, 5).forEach((item, index) => {
      assertEquals(destinations[index]!.displayName, getDisplayedName(item));
    });
    assertEquals('FooName', getDisplayedName(printerItems[5]!));
  });
});
