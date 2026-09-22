// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {LocalDestinationInfo, PrintPreviewDestinationDialogCrosElement, PrintPreviewDestinationListItemElement, PrintPreviewDestinationSettingsElement, RecentDestination} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, DestinationStoreEventType, makeRecentDestination, NativeLayerImpl, PDF_DESTINATION_KEY, State} from 'chrome://print/print_preview.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {getCddTemplate, setupTestListenerElement} from './print_preview_test_utils.js';

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
    setNativeLayerCrosInstance();

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
        false /* saveToDriveDisabled */,
        '' /* serializedDefaultDestinationSelectionRulesStr */);
    await whenCapabilitiesSet;
    await waitBeforeNextRender(destinationSettings);
  }

  async function openDialog() {
    await initSettings([]);
    destinationSettings.$.seeMore.click();
    await nativeLayer.whenCalled('getPrinters');
    await waitBeforeNextRender(destinationSettings);

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
      dialog: PrintPreviewDestinationDialogCrosElement,
      destination: Destination): Promise<void> {
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.printList.dispatchEvent(new CustomEvent('destination-selected', {
      detail: {destination} as unknown as
          PrintPreviewDestinationListItemElement,
    }));
    await whenClosed;
    await waitBeforeNextRender(destinationSettings);
  }

  test('RestoreFocusAfterDestinationChange', async () => {
    const destinations = [
      new Destination('FooDevice', DestinationOrigin.CROS, 'FooName'),
      new Destination('BarDevice', DestinationOrigin.CROS, 'BarName'),
    ];
    await initSettings(destinations.map(d => makeRecentDestination(d)));

    const dropdown = destinationSettings.$.destinationSelect;
    const select = dropdown.$.dropdown.$.destinationDropdown;
    select.focus();

    assertEquals(select, getDeepActiveElement());

    // Change destination to BarDevice.
    const whenCapabilitiesReady = eventToPromise(
        DestinationStoreEventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        destinationSettings.getDestinationStoreForTest());
    dropdown.dispatchEvent(new CustomEvent('selected-option-change', {
      bubbles: true,
      composed: true,
      detail: `BarDevice/${DestinationOrigin.CROS}/`,
    }));
    await whenCapabilitiesReady;
    await waitBeforeNextRender(dropdown);

    // Focus is restored to select after capabilities load.
    assertEquals(select, getDeepActiveElement());
  });

  test('RestoreFocusAfterDestinationDialogDismiss', async () => {
    const dialog = await openDialog();
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.dialog.cancel();
    await whenClosed;
    await waitBeforeNextRender(destinationSettings);

    assertEquals(destinationSettings.$.seeMore, getDeepActiveElement());
  });

  test('RestoreFocusAfterDestinationDialogSelectCached', async () => {
    const dialog = await openDialog();
    const destination = getDestination(`BarDevice/${DestinationOrigin.CROS}/`);
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
        dialog, getDestination(PDF_DESTINATION_KEY));
    await whenCapabilitiesReady;
    await waitBeforeNextRender(destinationSettings);

    assertEquals(destinationSettings.$.seeMore, getDeepActiveElement());
  });
});
