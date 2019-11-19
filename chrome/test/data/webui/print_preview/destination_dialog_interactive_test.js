// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, makeRecentDestination, NativeLayer, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {CloudPrintInterfaceStub} from 'chrome://test/print_preview/cloud_print_interface_stub.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {getDestinations, setupTestListenerElement} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise, fakeDataBind} from 'chrome://test/test_util.m.js';

window.destination_dialog_interactive_test = {};
destination_dialog_interactive_test.suiteName =
    'DestinationDialogInteractiveTest';
/** @enum {string} */
destination_dialog_interactive_test.TestNames = {
  FocusSearchBox: 'focus search box',
  FocusSearchBoxOnSignIn: 'focus search box on sign in',
  EscapeSearchBox: 'escape search box',
};

suite(destination_dialog_interactive_test.suiteName, function() {
  /** @type {?PrintPreviewDestinationDialogElement} */
  let dialog = null;

  /** @type {?NativeLayer} */
  let nativeLayer = null;

  /** @override */
  suiteSetup(function() {
    setupTestListenerElement();
  });

  /** @override */
  setup(function() {
    PolymerTest.clearBody();

    // Create destinations.
    nativeLayer = new NativeLayerStub();
    NativeLayer.setInstance(nativeLayer);
    const localDestinations = [];
    const destinations = getDestinations(nativeLayer, localDestinations);
    const recentDestinations = [makeRecentDestination(destinations[4])];
    nativeLayer.setLocalDestinations(localDestinations);
    const cloudPrintInterface = new CloudPrintInterfaceStub();

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
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        [] /* userAccounts */, true /* syncAvailable */);
    return nativeLayer.whenCalled('getPrinterCapabilities').then(() => {
      // Retrieve a reference to dialog
      dialog = destinationSettings.$.destinationDialog.get();
    });
  });

  // Tests that the search input text field is automatically focused when the
  // dialog is shown.
  test(
      assert(destination_dialog_interactive_test.TestNames.FocusSearchBox),
      function() {
        const searchInput = dialog.$.searchBox.getSearchInput();
        assertTrue(!!searchInput);
        const whenFocusDone = eventToPromise('focus', searchInput);
        dialog.destinationStore.startLoadAllDestinations();
        dialog.show();
        return whenFocusDone;
      });

  // Tests that the search input text field is automatically focused when the
  // user signs in successfully after clicking the sign in link. See
  // https://crbug.com/924921
  test(
      assert(
          destination_dialog_interactive_test.TestNames.FocusSearchBoxOnSignIn),
      function() {
        const searchInput = dialog.$.searchBox.getSearchInput();
        assertTrue(!!searchInput);
        const signInLink = dialog.$$('.sign-in');
        assertTrue(!!signInLink);
        const whenFocusDone = eventToPromise('focus', searchInput);
        dialog.destinationStore.startLoadAllDestinations();
        dialog.show();
        return whenFocusDone
            .then(() => {
              signInLink.focus();
              nativeLayer.setSignIn([]);
              signInLink.click();
              return nativeLayer.whenCalled('signIn');
            })
            .then(() => {
              // Link stays focused until successful signin.
              // See https://crbug.com/979603.
              assertEquals(signInLink, dialog.shadowRoot.activeElement);
              nativeLayer.setSignIn(['foo@chromium.org']);
              const whenSearchFocused = eventToPromise('focus', searchInput);
              signInLink.click();
              return whenSearchFocused;
            })
            .then(() => {
              assertEquals('foo@chromium.org', dialog.activeUser);
              assertEquals(1, dialog.users.length);
            });
      });

  // Tests that pressing the escape key while the search box is focused
  // closes the dialog if and only if the query is empty.
  test(
      assert(destination_dialog_interactive_test.TestNames.EscapeSearchBox),
      function() {
        const searchInput = dialog.$.searchBox.getSearchInput();
        assertTrue(!!searchInput);
        const whenFocusDone = eventToPromise('focus', searchInput);
        dialog.destinationStore.startLoadAllDestinations();
        dialog.show();
        return whenFocusDone
            .then(() => {
              assertTrue(dialog.$.dialog.open);

              // Put something in the search box.
              const whenSearchChanged =
                  eventToPromise('search-changed', dialog.$.searchBox);
              dialog.$.searchBox.setValue('query');
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
              assertTrue(dialog.$.dialog.open);

              // Clear the search box.
              const whenSearchChanged =
                  eventToPromise('search-changed', dialog.$.searchBox);
              dialog.$.searchBox.setValue('');
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
              assertFalse(dialog.$.dialog.open);
            });
      });
});
