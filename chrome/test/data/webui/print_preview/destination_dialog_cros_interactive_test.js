// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterfaceImpl, Destination, makeRecentDestination, NativeLayerImpl, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise, fakeDataBind} from '../test_util.m.js';

import {CloudPrintInterfaceStub} from './cloud_print_interface_stub.js';
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {getDestinations, setupTestListenerElement} from './print_preview_test_utils.js';

window.destination_dialog_cros_interactive_test = {};
const destination_dialog_cros_interactive_test =
    window.destination_dialog_cros_interactive_test;
destination_dialog_cros_interactive_test.suiteName =
    'DestinationDialogCrosInteractiveTest';
/** @enum {string} */
destination_dialog_cros_interactive_test.TestNames = {
  FocusSearchBox: 'focus search box',
  EscapeSearchBox: 'escape search box',
};

suite(destination_dialog_cros_interactive_test.suiteName, function() {
  /** @type {!PrintPreviewDestinationDialogCrosElement} */
  let dialog;

  /** @type {!NativeLayerStub} */
  let nativeLayer;

  /** @override */
  suiteSetup(function() {
    setupTestListenerElement();
  });

  /** @override */
  setup(function() {
    document.body.innerHTML = '';

    // Create destinations.
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    setNativeLayerCrosInstance();
    const localDestinations = [];
    const destinations = getDestinations(localDestinations);
    const recentDestinations = [makeRecentDestination(destinations[4])];
    nativeLayer.setLocalDestinations(localDestinations);
    const cloudPrintInterface = new CloudPrintInterfaceStub();
    CloudPrintInterfaceImpl.instance_ = cloudPrintInterface;
    cloudPrintInterface.configure();

    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    // Create destination settings, so  that the user manager is created.
    const destinationSettings =
        document.createElement('print-preview-destination-settings');
    destinationSettings.settings = model.settings;
    destinationSettings.state = State.READY;
    destinationSettings.disabled = false;
    fakeDataBind(model, destinationSettings, 'settings');
    document.body.appendChild(destinationSettings);

    // Initialize
    destinationSettings.cloudPrintInterface = cloudPrintInterface;
    destinationSettings.init(
        'FooDevice' /* printerName */, false /* pdfPrinterDisabled */,
        true /* isDriveMounted */,
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        [] /* userAccounts */, true /* syncAvailable */);
    return nativeLayer.whenCalled('getPrinterCapabilities').then(() => {
      // Retrieve a reference to dialog
      dialog = /** @type {!PrintPreviewDestinationDialogCrosElement} */ (
          destinationSettings.$$('#destinationDialog').get());
    });
  });

  // Tests that the search input text field is automatically focused when the
  // dialog is shown.
  test(
      assert(destination_dialog_cros_interactive_test.TestNames.FocusSearchBox),
      function() {
        const searchInput = /** @type {!PrintPreviewSearchBoxElement} */ (
                                dialog.$$('#searchBox'))
                                .getSearchInput();
        assertTrue(!!searchInput);
        const whenFocusDone = eventToPromise('focus', searchInput);
        dialog.destinationStore.startLoadAllDestinations();
        dialog.show();
        return whenFocusDone;
      });

  // Tests that pressing the escape key while the search box is focused
  // closes the dialog if and only if the query is empty.
  test(
      assert(
          destination_dialog_cros_interactive_test.TestNames.EscapeSearchBox),
      function() {
        const searchBox = /** @type {!PrintPreviewSearchBoxElement} */ (
            dialog.$$('#searchBox'));
        const searchInput = searchBox.getSearchInput();
        assertTrue(!!searchInput);
        const whenFocusDone = eventToPromise('focus', searchInput);
        dialog.destinationStore.startLoadAllDestinations();
        dialog.show();
        return whenFocusDone
            .then(() => {
              assertTrue(dialog.$$('#dialog').open);

              // Put something in the search box.
              const whenSearchChanged =
                  eventToPromise('search-changed', searchBox);
              searchBox.setValue('query');
              return whenSearchChanged;
            })
            .then(() => {
              assertEquals('query', searchInput.value);

              // Simulate escape
              const whenKeyDown = eventToPromise('keydown', dialog);
              keyDownOn(searchInput, 19, [], 'Escape');
              return whenKeyDown;
            })
            .then(() => {
              // Dialog should still be open.
              assertTrue(dialog.$$('#dialog').open);

              // Clear the search box.
              const whenSearchChanged =
                  eventToPromise('search-changed', searchBox);
              searchBox.setValue('');
              return whenSearchChanged;
            })
            .then(() => {
              assertEquals('', searchInput.value);

              // Simulate escape
              const whenKeyDown = eventToPromise('keydown', dialog);
              keyDownOn(searchInput, 19, [], 'Escape');
              return whenKeyDown;
            })
            .then(() => {
              // Dialog is closed.
              assertFalse(dialog.$$('#dialog').open);
            });
      });
});
