// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {LocalDestinationInfo, PrintPreviewDestinationDialogElement, PrintPreviewDestinationSettingsElement, RecentDestination} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, DestinationStoreEventType, makeRecentDestination, NativeLayerImpl, State} from 'chrome://print/print_preview.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate} from './print_preview_test_utils.js';
import {setupTestListenerElement} from './test_listener.js';

suite('DestinationSettingsInteractiveTest', function() {
  let destinationSettings: PrintPreviewDestinationSettingsElement;
  let nativeLayer: NativeLayerStub;

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);

    const localDestinations: LocalDestinationInfo[] = [
      {deviceName: 'FooDevice', printerName: 'FooName'},
      {deviceName: 'BarDevice', printerName: 'BarName'},
    ];
    nativeLayer.setLocalDestinations(localDestinations);

    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    destinationSettings =
        document.createElement('print-preview-destination-settings');
    destinationSettings.state = State.READY;
    destinationSettings.disabled = false;
    document.body.appendChild(destinationSettings);
  });

  async function initSettings(recentDestinations: RecentDestination[]) {
    destinationSettings.setSetting('recentDestinations', recentDestinations);

    const whenCapabilitiesSet = eventToPromise(
        DestinationStoreEventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        destinationSettings.getDestinationStoreForTest());
    destinationSettings.init(
        'FooDevice' /* printerName */, false /* pdfPrinterDisabled */,
        '' /* serializedDefaultDestinationSelectionRulesStr */);
    await whenCapabilitiesSet;
    await microtasksFinished();
  }

  async function openDialog() {
    await initSettings([]);
    destinationSettings.$.seeMore.click();
    await nativeLayer.whenCalled('getPrinters');
    await microtasksFinished();

    const dialog = destinationSettings.$.destinationDialog.getIfExists();
    assertTrue(!!dialog);
    assertTrue(dialog.isOpen());
    return dialog;
  }

  function getDestination(key: string): Destination {
    const destination =
        destinationSettings.getDestinationStoreForTest().getDestinationByKey(
            key);
    assertTrue(!!destination);
    return destination;
  }

  async function selectDestinationFromDialog(
      dialog: PrintPreviewDestinationDialogElement,
      destination: Destination): Promise<void> {
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.printList.dispatchEvent(
        new CustomEvent('destination-selected', {detail: destination}));
    await whenClosed;
    await microtasksFinished();
  }

  test('RestoreFocusAfterDestinationChange', async () => {
    const destinations = [
      new Destination('FooDevice', DestinationOrigin.LOCAL, 'FooName'),
      new Destination('BarDevice', DestinationOrigin.LOCAL, 'BarName'),
    ];
    await initSettings(destinations.map(d => makeRecentDestination(d)));

    const dropdown = destinationSettings.$.destinationSelect;
    const select = dropdown.$.select;
    select.focus();
    await microtasksFinished();

    assertEquals(select, getDeepActiveElement());

    // Change destination to BarDevice.
    const whenCapabilitiesReady = eventToPromise(
        DestinationStoreEventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        destinationSettings.getDestinationStoreForTest());
    dropdown.fire(
        'selected-option-change', `BarDevice/${DestinationOrigin.LOCAL}/`);
    await whenCapabilitiesReady;
    await microtasksFinished();

    // Focus is restored to select after capabilities load.
    assertEquals(select, getDeepActiveElement());
  });

  test('RestoreFocusAfterDestinationDialogDismiss', async () => {
    const dialog = await openDialog();
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.dialog.cancel();
    await whenClosed;
    await microtasksFinished();

    assertEquals(destinationSettings.$.seeMore, getDeepActiveElement());
  });

  test('RestoreFocusAfterDestinationDialogSelectCached', async () => {
    const dialog = await openDialog();
    const destination = getDestination('BarDevice/local/');
    destination.capabilities =
        getCddTemplate('BarDevice', 'BarName').capabilities;

    await selectDestinationFromDialog(dialog, destination);

    assertEquals(destinationSettings.$.seeMore, getDeepActiveElement());
  });

  test('RestoreFocusAfterDestinationDialogSelectUncached', async () => {
    const dialog = await openDialog();

    const whenCapabilitiesReady = eventToPromise(
        DestinationStoreEventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        destinationSettings.getDestinationStoreForTest());
    await selectDestinationFromDialog(
        dialog, getDestination('BarDevice/local/'));
    await whenCapabilitiesReady;
    await microtasksFinished();

    assertEquals(destinationSettings.$.seeMore, getDeepActiveElement());
  });
});
