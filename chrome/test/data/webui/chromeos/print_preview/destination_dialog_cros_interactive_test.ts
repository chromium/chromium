// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewDestinationDialogCrosElement, PrintPreviewDestinationListItemElement} from 'chrome://print/print_preview.js';
import {NativeLayerImpl, State} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {setupTestListenerElement} from './print_preview_test_utils.js';

suite('DestinationDialogInteractiveTest', function() {
  let dialog: PrintPreviewDestinationDialogCrosElement;

  let nativeLayer: NativeLayerStub;

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Create destinations.
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    setNativeLayerCrosInstance();

    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);
  });

  async function createDialog(defaultId: string = 'FooDevice') {
    // Create destination settings, so  that the user manager is created.
    const model = document.querySelector('print-preview-model')!;
    const destinationSettings =
        document.createElement('print-preview-destination-settings');
    destinationSettings.settings = (model as any).settings;
    destinationSettings.state = State.READY;
    destinationSettings.disabled = false;
    fakeDataBind(model, destinationSettings, 'settings');
    document.body.appendChild(destinationSettings);

    // Initialize
    destinationSettings.init(
        defaultId /* printerName */, false /* pdfPrinterDisabled */,
        false /* saveToDriveDisabled */,
        '' /* serializedDefaultDestinationSelectionRulesStr */);
    await nativeLayer.whenCalled('getPrinterCapabilities');
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

    // Retrieve a reference to dialog
    dialog = destinationSettings.$.destinationDialog.get();
  }

  // Tests that the search input text field is automatically focused when the
  // dialog is shown and there are destinations available.
  test('FocusSearchBox', async () => {
    await createDialog();
    const searchInput = dialog.$.searchBox.getSearchInput();
    assertTrue(!!searchInput);
    const whenFocusDone = eventToPromise('focus', searchInput);
    dialog.destinationStore.startLoadAllDestinations();
    dialog.show();
    return whenFocusDone;
  });

  // Tests that pressing the escape key while the search box is focused
  // closes the dialog if and only if the query is empty.
  test('EscapeSearchBox', async () => {
    await createDialog();
    const searchBox = dialog.$.searchBox;
    const searchInput = searchBox.getSearchInput();
    assertTrue(!!searchInput);
    const whenFocusDone = eventToPromise('focus', searchInput);
    dialog.destinationStore.startLoadAllDestinations();
    dialog.show();
    await whenFocusDone;
    assertTrue(dialog.$.dialog.open);

    // Put something in the search box.
    const whenSearchChanged = eventToPromise('search-changed', searchBox);
    searchBox.setValue('query');
    await whenSearchChanged;
    assertEquals('query', searchInput.value);

    // Simulate escape
    let whenKeyDown = eventToPromise('keydown', dialog);
    keyDownOn(searchInput, 19, [], 'Escape');
    await whenKeyDown;

    // Dialog should still be open.
    assertTrue(dialog.$.dialog.open);

    // Clear the search box.
    const whenSearchChanged2 = eventToPromise('search-changed', searchBox);
    searchBox.setValue('');
    await whenSearchChanged2;
    assertEquals('', searchInput.value);

    // Simulate escape
    whenKeyDown = eventToPromise('keydown', dialog);
    keyDownOn(searchInput, 19, [], 'Escape');
    await whenKeyDown;

    // Dialog is closed.
    assertFalse(dialog.$.dialog.open);
  });

  test('SearchDestinationsKorean', async () => {
    const koreanNameDestinations = [
      {
        deviceName: 'id1',
        printerName: '한국',
        printerDescription: '한국',
        printerOptions: {location: '대구'},
      },
      {
        deviceName: 'id2',
        printerName: '김밥',
        printerDescription: '한국',
        printerOptions: {location: '서울'},
      },
    ];
    nativeLayer.setLocalDestinations(koreanNameDestinations);
    await createDialog('id1');

    const searchBox = dialog.$.searchBox;
    const searchInput = searchBox.getSearchInput();
    assertTrue(!!searchInput);
    const whenFocusDone = eventToPromise('focus', searchInput);
    dialog.destinationStore.startLoadAllDestinations();
    dialog.show();
    await whenFocusDone;
    assertTrue(dialog.$.dialog.open);

    async function queryPrinters(query: string):
        Promise<NodeListOf<PrintPreviewDestinationListItemElement>> {
      const whenSearchChanged = eventToPromise('search-changed', searchBox);
      searchBox.setValue(query);
      await whenSearchChanged;
      await microtasksFinished();

      const list =
          dialog.shadowRoot!.querySelector('print-preview-destination-list')!;
      if (list.shadowRoot!.querySelector('iron-list')!.hasAttribute('hidden')) {
        return list.shadowRoot!.querySelectorAll(
            'print-preview-destination-list-item:not(*)');
      }
      return list.shadowRoot!.querySelectorAll(
          'print-preview-destination-list-item');
    }

    // Search for the name of the second printer.
    let listItems = await queryPrinters(koreanNameDestinations[1]!.printerName);
    assertEquals(1, listItems.length);
    assertEquals('id2', listItems[0]!.destination.id);

    // Search for the location of the first printer.
    listItems =
        await queryPrinters(koreanNameDestinations[0]!.printerOptions.location);
    assertEquals(1, listItems.length);
    assertEquals('id1', listItems[0]!.destination.id);

    // Search for the description, which is the same for both printers.
    listItems =
        await queryPrinters(koreanNameDestinations[0]!.printerDescription);
    assertEquals(2, listItems.length);
    assertEquals('id1', listItems[0]!.destination.id);
    assertEquals('id2', listItems[1]!.destination.id);

    // Search for something that doesn't match either printer.
    listItems = await queryPrinters('한강;');
    assertEquals(0, listItems.length);
  });
});
