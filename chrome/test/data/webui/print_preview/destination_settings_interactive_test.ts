// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {LocalDestinationInfo, PrintPreviewDestinationSettingsElement, RecentDestination} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin, DestinationStoreEventType, makeRecentDestination, NativeLayerImpl, State} from 'chrome://print/print_preview.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
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

  test('RestoreFocusAfterDestinationChange', async () => {
    const destinations = [
      new Destination('FooDevice', DestinationOrigin.LOCAL, 'FooName'),
      new Destination('BarDevice', DestinationOrigin.LOCAL, 'BarName'),
    ];
    const recentDestinations: RecentDestination[] =
        destinations.map(d => makeRecentDestination(d));
    destinationSettings.setSetting('recentDestinations', recentDestinations);

    const whenCapabilitiesSet = eventToPromise(
        DestinationStoreEventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        destinationSettings.getDestinationStoreForTest());
    destinationSettings.init(
        'FooDevice' /* printerName */, false /* pdfPrinterDisabled */,
        '' /* serializedDefaultDestinationSelectionRulesStr */);
    await whenCapabilitiesSet;
    await microtasksFinished();

    const dropdown = destinationSettings.$.destinationSelect;
    const select = dropdown.$.select;
    select.focus();
    await microtasksFinished();

    assertEquals(select, getDeepActiveElement());

    // Change destination to BarDevice.
    const whenCapabilitiesReady = eventToPromise(
        DestinationStoreEventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        destinationSettings.getDestinationStoreForTest());
    dropdown.dispatchEvent(new CustomEvent('selected-option-change', {
      bubbles: true,
      composed: true,
      detail: `BarDevice/${DestinationOrigin.LOCAL}/`,
    }));
    await whenCapabilitiesReady;
    await microtasksFinished();

    // Focus is restored to select after capabilities load.
    assertEquals(select, getDeepActiveElement());
  });
});
