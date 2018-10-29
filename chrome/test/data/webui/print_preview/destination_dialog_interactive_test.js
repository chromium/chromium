// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_dialog_interactive_test', function() {
  /** @enum {string} */
  const TestNames = {
    FocusSearchBox: 'focus search box',
    EscapeSearchBox: 'escape search box',
  };

  const suiteName = 'DestinationDialogInteractiveTest';

  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationDialogElement} */
    let dialog = null;

    /** @type {?print_preview.DestinationStore} */
    let destinationStore = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    /** @override */
    setup(function() {
      // Create destinations.
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      const userInfo = new print_preview.UserInfo();
      destinationStore = new print_preview.DestinationStore(
          userInfo, new WebUIListenerTracker());
      const localDestinations = [];
      const destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
      const recentDestinations =
          [print_preview.makeRecentDestination(destinations[4])];
      destinationStore.init(
          false /* isInAppKioskMode */, 'FooDevice' /* printerName */,
          '' /* serializedDefaultDestinationSelectionRulesStr */,
          recentDestinations /* recentDestinations */);
      nativeLayer.setLocalDestinations(localDestinations);

      // Set up dialog
      dialog = document.createElement('print-preview-destination-dialog');
      dialog.userInfo = userInfo;
      dialog.destinationStore = destinationStore;
      dialog.invitationStore = new print_preview.InvitationStore(userInfo);
      dialog.recentDestinations = recentDestinations;
      document.body.appendChild(dialog);
      return nativeLayer.whenCalled('getPrinterCapabilities');
    });

    // Tests that the search input text field is automatically focused when the
    // dialog is shown.
    test(assert(TestNames.FocusSearchBox), function() {
      const searchInput = dialog.$.searchBox.getSearchInput();
      assertTrue(!!searchInput);
      const whenFocusDone = test_util.eventToPromise('focus', searchInput);
      destinationStore.startLoadAllDestinations();
      dialog.show();
      return whenFocusDone;
    });

    // Tests that pressing the escape key while the search box is focused
    // closes the dialog if and only if the query is empty.
    test(assert(TestNames.EscapeSearchBox), function() {
      const searchInput = dialog.$.searchBox.getSearchInput();
      assertTrue(!!searchInput);
      const whenFocusDone = test_util.eventToPromise('focus', searchInput);
      destinationStore.startLoadAllDestinations();
      dialog.show();
      return whenFocusDone
          .then(() => {
            assertTrue(dialog.$.dialog.open);

            // Put something in the search box.
            const whenSearchChanged =
                test_util.eventToPromise('search-changed', dialog.$.searchBox);
            dialog.$.searchBox.setValue('query');
            return whenSearchChanged;
          })
          .then(() => {
            assertEquals('query', searchInput.value);

            // Simulate escape
            const whenKeyDown = test_util.eventToPromise('keydown', dialog);
            MockInteractions.keyDownOn(searchInput, 19, [], 'Escape');
            return whenKeyDown;
          })
          .then(() => {
            // Dialog should still be open.
            assertTrue(dialog.$.dialog.open);

            // Clear the search box.
            const whenSearchChanged =
                test_util.eventToPromise('search-changed', dialog.$.searchBox);
            dialog.$.searchBox.setValue('');
            return whenSearchChanged;
          })
          .then(() => {
            assertEquals('', searchInput.value);

            // Simulate escape
            const whenKeyDown = test_util.eventToPromise('keydown', dialog);
            MockInteractions.keyDownOn(searchInput, 19, [], 'Escape');
            return whenKeyDown;
          })
          .then(() => {
            // Dialog is closed.
            assertFalse(dialog.$.dialog.open);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
