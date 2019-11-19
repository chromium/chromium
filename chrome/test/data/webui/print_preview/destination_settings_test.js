// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationErrorType, DestinationOrigin, DestinationState, DestinationStore, DestinationType, Error, makeRecentDestination, NativeLayer, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CloudPrintInterfaceStub} from 'chrome://test/print_preview/cloud_print_interface_stub.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {getDestinations, getGoogleDriveDestination, getSaveAsPdfDestination, setupTestListenerElement} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise, fakeDataBind, waitBeforeNextRender} from 'chrome://test/test_util.m.js';

window.destination_settings_test = {};
destination_settings_test.suiteName = 'DestinationSettingsTest';
/** @enum {string} */
destination_settings_test.TestNames = {
  ChangeDropdownState: 'change dropdown state',
  NoRecentDestinations: 'no recent destinations',
  RecentDestinations: 'recent destinations',
  SaveAsPdfRecent: 'save as pdf recent',
  GoogleDriveRecent: 'google drive recent',
  SelectSaveAsPdf: 'select save as pdf',
  SelectGoogleDrive: 'select google drive',
  SelectRecentDestination: 'select recent destination',
  OpenDialog: 'open dialog',
  TwoAccountsRecentDestinations: 'two accounts recent destinations',
  UpdateRecentDestinations: 'update recent destinations',
  ResetDestinationOnSignOut: 'reset destination on sign out',
  DisabledSaveAsPdf: 'disabled save as pdf',
  NoDestinations: 'no destinations'
};

suite(destination_settings_test.suiteName, function() {
  /** @type {?PrintPreviewDestinationSettingsElement} */
  let destinationSettings = null;

  /** @type {?NativeLayer} */
  let nativeLayer = null;

  /** @type {?CloudPrintInterface} */
  let cloudPrintInterface = null;

  /** @type {!Array<!RecentDestination>} */
  let recentDestinations = [];

  /** @type {!Array<!Destination>} */
  let destinations = [];

  /** @type {!Array<string>} */
  let initialAccounts = [];

  /** @type {boolean} */
  let pdfPrinterDisabled = false;

  /** @type {string} */
  const defaultUser = 'foo@chromium.org';

  /** @override */
  suiteSetup(function() {
    setupTestListenerElement();
  });

  /** @override */
  setup(function() {
    PolymerTest.clearBody();

    // Stub out native layer and cloud print interface.
    nativeLayer = new NativeLayerStub();
    NativeLayer.setInstance(nativeLayer);
    const localDestinations = [];
    destinations = getDestinations(nativeLayer, localDestinations);
    nativeLayer.setLocalDestinations(localDestinations);
    cloudPrintInterface = new CloudPrintInterfaceStub();

    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    destinationSettings =
        document.createElement('print-preview-destination-settings');
    destinationSettings.settings = model.settings;
    destinationSettings.state = State.NOT_READY;
    destinationSettings.disabled = true;
    fakeDataBind(model, destinationSettings, 'settings');
    document.body.appendChild(destinationSettings);
  });

  // Tests that the dropdown is enabled or disabled correctly based on
  // the state.
  test(
      assert(destination_settings_test.TestNames.ChangeDropdownState),
      function() {
        const dropdown = destinationSettings.$.destinationSelect;
        // Initial state: No destination store means that there is no
        // destination yet, so the dropdown is hidden.
        assertTrue(dropdown.hidden);
        destinationSettings.cloudPrintInterface = cloudPrintInterface;

        // Set up the destination store, but no destination yet. Dropdown is
        // still hidden.
        destinationSettings.init(
            'FooDevice' /* printerName */,
            '' /* serializedDefaultDestinationSelectionRulesStr */,
            [] /* userAccounts */, true /* syncAvailable */);
        assertTrue(dropdown.hidden);

        return eventToPromise(
                   DestinationStore.EventType
                       .SELECTED_DESTINATION_CAPABILITIES_READY,
                   destinationSettings.destinationStore_)
            .then(() => {
              // The capabilities ready event results in |destinationState|
              // changing to SELECTED, which enables and shows the dropdown even
              // though |state| has not yet transitioned to READY. This is to
              // prevent brief losses of focus when the destination changes.
              assertFalse(dropdown.disabled);
              assertFalse(dropdown.hidden);
              destinationSettings.state = State.READY;
              destinationSettings.disabled = false;

              // Simulate setting a setting to an invalid value. Dropdown is
              // disabled due to validation error on another control.
              destinationSettings.state = State.ERROR;
              destinationSettings.disabled = true;
              assertTrue(dropdown.disabled);

              // Simulate the user fixing the validation error, and then
              // selecting an invalid printer. Dropdown is enabled, so that the
              // user can fix the error.
              destinationSettings.state = State.READY;
              destinationSettings.disabled = false;
              destinationSettings.destinationStore_.dispatchEvent(
                  new CustomEvent(
                      DestinationStore.EventType.ERROR,
                      {detail: DestinationErrorType.INVALID}));
              flush();

              assertEquals(
                  DestinationState.ERROR, destinationSettings.destinationState);
              assertEquals(Error.INVALID_PRINTER, destinationSettings.error);
              destinationSettings.state = State.ERROR;
              destinationSettings.disabled = true;
              assertFalse(dropdown.disabled);

              // Simulate the user having no printers.
              destinationSettings.destinationStore_.dispatchEvent(
                  new CustomEvent(
                      DestinationStore.EventType.ERROR,
                      {detail: DestinationErrorType.NO_DESTINATIONS}));
              flush();

              assertEquals(
                  DestinationState.ERROR, destinationSettings.destinationState);
              assertEquals(Error.NO_DESTINATIONS, destinationSettings.error);
              destinationSettings.state = State.FATAL_ERROR;
              destinationSettings.disabled = true;
              assertTrue(dropdown.disabled);
            });
      });

  /** @return {!DestinationOrigin} */
  function getLocalOrigin() {
    return isChromeOS ? DestinationOrigin.CROS : DestinationOrigin.LOCAL;
  }

  /**
   * Initializes the destination store and destination settings using
   * |destinations| and |recentDestinations|.
   */
  function initialize() {
    // Initialize destination settings.
    destinationSettings.cloudPrintInterface = cloudPrintInterface;
    destinationSettings.setSetting('recentDestinations', recentDestinations);
    destinationSettings.appKioskMode = false;
    destinationSettings.init(
        '' /* printerName */, pdfPrinterDisabled,
        '' /* serializedDefaultDestinationSelectionRulesStr */, initialAccounts,
        true /* syncAvailable */);
    destinationSettings.state = State.READY;
    destinationSettings.disabled = false;
  }

  /** Simulates a user signing in to Chrome. */
  function signIn() {
    cloudPrintInterface.setPrinter(getGoogleDriveDestination(defaultUser));
    window.cr.webUIListenerCallback('user-accounts-updated', [defaultUser]);
    flush();
  }

  /**
   * @param {string} id The id of the local destination.
   * @return {string} The key corresponding to the local destination, with the
   *     origin set correctly based on the platform.
   */
  function makeLocalDestinationKey(id) {
    return id + '/' + getLocalOrigin() + '/';
  }

  /**
   * @param {!Array<string>} expectedDestinations An array of the expected
   *     destinations in the dropdown.
   */
  function assertDropdownItems(expectedDestinations) {
    const options =
        destinationSettings.$.destinationSelect.shadowRoot.querySelectorAll(
            'option:not([hidden])');
    assertEquals(expectedDestinations.length + 1, options.length);
    expectedDestinations.forEach((expectedValue, index) => {
      assertEquals(expectedValue, options[index].value);
    });
    assertEquals('seeMore', options[expectedDestinations.length].value);
  }

  // Tests that the dropdown contains the appropriate destinations when there
  // are no recent destinations.
  test(
      assert(destination_settings_test.TestNames.NoRecentDestinations),
      function() {
        initialize();
        return nativeLayer.whenCalled('getPrinterCapabilities')
            .then(() => {
              // This will result in the destination store setting the Save as
              // PDF destination.
              assertEquals(
                  Destination.GooglePromotedId.SAVE_AS_PDF,
                  destinationSettings.destination.id);
              assertFalse(destinationSettings.$.destinationSelect.disabled);
              assertDropdownItems(['Save as PDF/local/']);

              // If the user is signed in, Save to Drive should be displayed.
              signIn();
              return waitBeforeNextRender(destinationSettings);
            })
            .then(() => {
              assertDropdownItems([
                'Save as PDF/local/',
                '__google__docs/cookies/foo@chromium.org',
              ]);
            });
      });

  // Tests that the dropdown contains the appropriate destinations when there
  // are 3 recent destinations.
  test(
      assert(destination_settings_test.TestNames.RecentDestinations),
      function() {
        recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));

        initialize();

        // Wait for the destinations to be inserted into the store.
        return nativeLayer.whenCalled('getPrinterCapabilities').then(() => {
          // This will result in the destination store setting the most recent
          // destination.
          assertEquals('ID1', destinationSettings.destination.id);
          assertFalse(destinationSettings.$.destinationSelect.disabled);
          assertDropdownItems([
            makeLocalDestinationKey('ID1'),
            makeLocalDestinationKey('ID2'),
            makeLocalDestinationKey('ID3'),
            'Save as PDF/local/',
          ]);

          // If the user is signed in, Save to Drive should be displayed.
          signIn();
          assertDropdownItems([
            makeLocalDestinationKey('ID1'),
            makeLocalDestinationKey('ID2'),
            makeLocalDestinationKey('ID3'),
            'Save as PDF/local/',
            '__google__docs/cookies/foo@chromium.org',
          ]);
        });
      });

  // Tests that the dropdown contains the appropriate destinations when Save
  // as PDF is one of the recent destinations.
  test(assert(destination_settings_test.TestNames.SaveAsPdfRecent), function() {
    recentDestinations = destinations.slice(0, 3).map(
        destination => makeRecentDestination(destination));
    recentDestinations.splice(
        1, 1, makeRecentDestination(getSaveAsPdfDestination()));
    initialize();

    return nativeLayer.whenCalled('getPrinterCapabilities').then(() => {
      // This will result in the destination store setting the most recent
      // destination.
      assertEquals('ID1', destinationSettings.destination.id);
      assertFalse(destinationSettings.$.destinationSelect.disabled);
      assertDropdownItems([
        makeLocalDestinationKey('ID1'),
        makeLocalDestinationKey('ID3'),
        'Save as PDF/local/',
      ]);

      // If the user is signed in, Save to Drive should be displayed.
      signIn();
      assertDropdownItems([
        makeLocalDestinationKey('ID1'),
        makeLocalDestinationKey('ID3'),
        'Save as PDF/local/',
        '__google__docs/cookies/foo@chromium.org',
      ]);
    });
  });

  // Tests that the dropdown contains the appropriate destinations when
  // Google Drive is in the recent destinations.
  test(
      assert(destination_settings_test.TestNames.GoogleDriveRecent),
      function() {
        recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));
        recentDestinations.splice(
            1, 1,
            makeRecentDestination(getGoogleDriveDestination(defaultUser)));
        initialize();

        return nativeLayer.whenCalled('getPrinterCapabilities').then(() => {
          // This will result in the destination store setting the most recent
          // destination.
          assertEquals('ID1', destinationSettings.destination.id);
          assertFalse(destinationSettings.$.destinationSelect.disabled);

          // Google Drive does not show up even though it is recent, since the
          // user is not signed in and the destination is not available.
          assertDropdownItems([
            makeLocalDestinationKey('ID1'),
            makeLocalDestinationKey('ID3'),
            'Save as PDF/local/',
          ]);

          // If the user is signed in, Save to Drive should be displayed.
          signIn();
          assertDropdownItems([
            makeLocalDestinationKey('ID1'),
            makeLocalDestinationKey('ID3'),
            'Save as PDF/local/',
            '__google__docs/cookies/foo@chromium.org',
          ]);
        });
      });

  // Tests that selecting the Save as PDF destination results in the
  // DESTINATION_SELECT event firing, with Save as PDF set as the current
  // destination.
  test(assert(destination_settings_test.TestNames.SelectSaveAsPdf), function() {
    recentDestinations = destinations.slice(0, 3).map(
        destination => makeRecentDestination(destination));
    recentDestinations.splice(
        1, 1, makeRecentDestination(getSaveAsPdfDestination()));
    initialize();

    const dropdown = destinationSettings.$.destinationSelect;

    return nativeLayer.whenCalled('getPrinterCapabilities')
        .then(() => {
          // This will result in the destination store setting the most recent
          // destination.
          assertEquals('ID1', destinationSettings.destination.id);
          assertFalse(dropdown.disabled);
          assertDropdownItems([
            makeLocalDestinationKey('ID1'),
            makeLocalDestinationKey('ID3'),
            'Save as PDF/local/',
          ]);
          // Most recent destination is selected by default.
          assertEquals('ID1', destinationSettings.destination.id);

          // Simulate selection of Save as PDF printer.
          const whenDestinationSelect = eventToPromise(
              DestinationStore.EventType.DESTINATION_SELECT,
              destinationSettings.destinationStore_);
          dropdown.fire('selected-option-change', 'Save as PDF/local/');

          // Ensure this fires the destination select event.
          return whenDestinationSelect;
        })
        .then(() => {
          assertEquals(
              Destination.GooglePromotedId.SAVE_AS_PDF,
              destinationSettings.destination.id);
        });
  });

  // Tests that selecting the Google Drive destination results in the
  // DESTINATION_SELECT event firing, with Google Drive set as the current
  // destination.
  test(
      assert(destination_settings_test.TestNames.SelectGoogleDrive),
      function() {
        recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));
        recentDestinations.splice(
            1, 1,
            makeRecentDestination(getGoogleDriveDestination(defaultUser)));
        initialize();
        const dropdown = destinationSettings.$.destinationSelect;

        return nativeLayer.whenCalled('getPrinterCapabilities')
            .then(() => {
              // This will result in the destination store setting the most
              // recent destination.
              assertEquals('ID1', destinationSettings.destination.id);
              assertFalse(dropdown.disabled);

              // If the user is signed in, Save to Drive should be displayed.
              signIn();
              assertDropdownItems([
                makeLocalDestinationKey('ID1'),
                makeLocalDestinationKey('ID3'),
                'Save as PDF/local/',
                '__google__docs/cookies/foo@chromium.org',
              ]);

              // Most recent destination is still selected.
              assertEquals('ID1', destinationSettings.destination.id);

              // Simulate selection of Google Drive printer.
              const whenDestinationSelect = eventToPromise(
                  DestinationStore.EventType.DESTINATION_SELECT,
                  destinationSettings.destinationStore_);
              dropdown.fire(
                  'selected-option-change',
                  '__google__docs/cookies/foo@chromium.org');
              return whenDestinationSelect;
            })
            .then(() => {
              assertEquals(
                  Destination.GooglePromotedId.DOCS,
                  destinationSettings.destination.id);
            });
      });

  // Tests that selecting a recent destination results in the
  // DESTINATION_SELECT event firing, with the recent destination set as the
  // current destination.
  test(
      assert(destination_settings_test.TestNames.SelectRecentDestination),
      function() {
        recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));
        initialize();
        const dropdown = destinationSettings.$.destinationSelect;

        return nativeLayer.whenCalled('getPrinterCapabilities')
            .then(() => {
              // This will result in the destination store setting the most
              // recent destination.
              assertEquals('ID1', destinationSettings.destination.id);
              assertFalse(dropdown.disabled);
              assertDropdownItems([
                makeLocalDestinationKey('ID1'),
                makeLocalDestinationKey('ID2'),
                makeLocalDestinationKey('ID3'),
                'Save as PDF/local/',
              ]);

              // Simulate selection of Save as PDF printer.
              const whenDestinationSelect = eventToPromise(
                  DestinationStore.EventType.DESTINATION_SELECT,
                  destinationSettings.destinationStore_);
              dropdown.fire(
                  'selected-option-change', makeLocalDestinationKey('ID2'));
              return whenDestinationSelect;
            })
            .then(() => {
              assertEquals('ID2', destinationSettings.destination.id);
            });
      });

  // Tests that selecting the 'see more' option opens the dialog.
  test(assert(destination_settings_test.TestNames.OpenDialog), function() {
    recentDestinations = destinations.slice(0, 3).map(
        destination => makeRecentDestination(destination));
    initialize();
    const dropdown = destinationSettings.$.destinationSelect;

    return nativeLayer.whenCalled('getPrinterCapabilities')
        .then(() => {
          // This will result in the destination store setting the most recent
          // destination.
          assertEquals('ID1', destinationSettings.destination.id);
          assertFalse(dropdown.disabled);
          assertDropdownItems([
            makeLocalDestinationKey('ID1'),
            makeLocalDestinationKey('ID2'),
            makeLocalDestinationKey('ID3'),
            'Save as PDF/local/',
          ]);

          dropdown.fire('selected-option-change', 'seeMore');
          return waitBeforeNextRender(destinationSettings);
        })
        .then(() => {
          assertTrue(destinationSettings.$$('print-preview-destination-dialog')
                         .isOpen());
        });
  });

  test(
      assert(destination_settings_test.TestNames.TwoAccountsRecentDestinations),
      function() {
        const account2 = 'bar@chromium.org';
        const driveUser1 = getGoogleDriveDestination(defaultUser);
        const driveUser2 = getGoogleDriveDestination(account2);
        const cloudPrinterUser1 = new Destination(
            'FooCloud', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
            'FooCloudName', DestinationConnectionStatus.ONLINE,
            {account: defaultUser});
        const cloudPrinterUser2 = new Destination(
            'BarCloud', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
            'BarCloudName', DestinationConnectionStatus.ONLINE,
            {account: account2});
        cloudPrintInterface.setPrinter(getGoogleDriveDestination(defaultUser));
        cloudPrintInterface.setPrinter(driveUser2);
        cloudPrintInterface.setPrinter(cloudPrinterUser1);
        cloudPrintInterface.setPrinter(cloudPrinterUser2);

        recentDestinations = [
          cloudPrinterUser1, cloudPrinterUser2, destinations[0]
        ].map(destination => makeRecentDestination(destination));

        initialAccounts = [defaultUser, account2];
        initialize();
        flush();

        const dropdown = destinationSettings.$.destinationSelect;

        return cloudPrintInterface.whenCalled('printer')
            .then(() => {
              // This will result in the destination store setting the most
              // recent destination.
              assertEquals('FooCloud', destinationSettings.destination.id);
              assertFalse(dropdown.disabled);
              assertDropdownItems([
                'FooCloud/cookies/foo@chromium.org',
                makeLocalDestinationKey('ID1'),
                'Save as PDF/local/',
                '__google__docs/cookies/foo@chromium.org',
              ]);

              dropdown.fire('selected-option-change', 'seeMore');
              return waitBeforeNextRender(destinationSettings);
            })
            .then(() => {
              const dialog =
                  destinationSettings.$$('print-preview-destination-dialog');
              assertTrue(dialog.isOpen());
              const whenAdded = eventToPromise(
                  DestinationStore.EventType.DESTINATIONS_INSERTED,
                  destinationSettings.destinationStore_);
              // Simulate setting a new account.
              dialog.fire('account-change', account2);
              flush();
              return whenAdded;
            })
            .then(() => {
              assertDropdownItems([
                'BarCloud/cookies/bar@chromium.org',
                makeLocalDestinationKey('ID1'),
                'Save as PDF/local/',
                '__google__docs/cookies/bar@chromium.org',
              ]);
            });
      });

  /**
   * @param {!Array<string>} expectedDestinationIds An array of the expected
   *     recent destination ids.
   */
  function assertRecentDestinations(expectedDestinationIds) {
    const recentDestinations =
        destinationSettings.getSettingValue('recentDestinations');
    assertEquals(expectedDestinationIds.length, recentDestinations.length);
    expectedDestinationIds.forEach((expectedId, index) => {
      assertEquals(expectedId, recentDestinations[index].id);
    });
  }

  function selectDestination(destination) {
    destinationSettings.destinationStore_.selectDestination(destination);
    flush();
  }

  /**
   * Tests that the destination being set correctly updates the recent
   * destinations array.
   */
  test(
      assert(destination_settings_test.TestNames.UpdateRecentDestinations),
      function() {
        // Recent destinations start out empty.
        assertRecentDestinations([]);
        assertEquals(0, nativeLayer.getCallCount('getPrinterCapabilities'));

        initialize();

        return nativeLayer.whenCalled('getPrinterCapabilities')
            .then(() => {
              assertRecentDestinations(['Save as PDF']);
              assertEquals(
                  1, nativeLayer.getCallCount('getPrinterCapabilities'));

              // Simulate setting a destination.
              nativeLayer.resetResolver('getPrinterCapabilities');
              selectDestination(destinations[0]);
              return nativeLayer.whenCalled('getPrinterCapabilities');
            })
            .then(() => {
              assertRecentDestinations(['ID1', 'Save as PDF']);
              assertEquals(
                  1, nativeLayer.getCallCount('getPrinterCapabilities'));

              // Reselect a recent destination. Still 2 destinations, but in a
              // different order.
              nativeLayer.resetResolver('getPrinterCapabilities');
              destinationSettings.$.destinationSelect.dispatchEvent(
                  new CustomEvent('selected-option-change', {
                    detail: 'Save as PDF/local/',
                  }));
              flush();
              assertRecentDestinations(['Save as PDF', 'ID1']);
              // No additional capabilities call, since the destination was
              // previously selected.
              assertEquals(
                  0, nativeLayer.getCallCount('getPrinterCapabilities'));

              // Select a third destination
              selectDestination(destinations[1]);
              return nativeLayer.whenCalled('getPrinterCapabilities');
            })
            .then(() => {
              assertRecentDestinations(['ID2', 'Save as PDF', 'ID1']);
              assertEquals(
                  1, nativeLayer.getCallCount('getPrinterCapabilities'));

              // Select a fourth destination. List does not grow.
              nativeLayer.resetResolver('getPrinterCapabilities');
              selectDestination(destinations[2]);
              return nativeLayer.whenCalled('getPrinterCapabilities');
            })
            .then(() => {
              assertRecentDestinations(['ID3', 'ID2', 'Save as PDF']);
              assertEquals(
                  1, nativeLayer.getCallCount('getPrinterCapabilities'));
            });
      });

  // Tests that the dropdown resets the destination if the user signs out of
  // the account associated with the curret one.
  test(
      assert(destination_settings_test.TestNames.ResetDestinationOnSignOut),
      function() {
        recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));
        const driveDestination = getGoogleDriveDestination(defaultUser);
        recentDestinations.splice(
            0, 1, makeRecentDestination(driveDestination));
        cloudPrintInterface.setPrinter(getGoogleDriveDestination(defaultUser));
        initialAccounts = [defaultUser];
        initialize();

        return cloudPrintInterface.whenCalled('printer')
            .then(() => {
              assertEquals(
                  Destination.GooglePromotedId.DOCS,
                  destinationSettings.destination.id);
              assertFalse(destinationSettings.$.destinationSelect.disabled);
              assertDropdownItems([
                makeLocalDestinationKey('ID2'),
                makeLocalDestinationKey('ID3'),
                'Save as PDF/local/',
                '__google__docs/cookies/foo@chromium.org',
              ]);

              // Sign out.
              window.cr.webUIListenerCallback('user-accounts-updated', []);
              flush();

              return nativeLayer.whenCalled('getPrinterCapabilities');
            })
            .then(() => {
              assertEquals('ID2', destinationSettings.destination.id);
              assertFalse(destinationSettings.$.destinationSelect.disabled);
              assertDropdownItems([
                makeLocalDestinationKey('ID2'),
                makeLocalDestinationKey('ID3'),
                'Save as PDF/local/',
              ]);

              // Now that the selected destination is local, signing in and out
              // shouldn't impact it.
              signIn();
              assertEquals('ID2', destinationSettings.destination.id);

              window.cr.webUIListenerCallback('user-accounts-updated', []);
              flush();
              assertEquals('ID2', destinationSettings.destination.id);
            });
      });

  // Tests that disabling the Save as PDF destination hides the corresponding
  // dropdown item.
  test(
      assert(destination_settings_test.TestNames.DisabledSaveAsPdf),
      function() {
        // Initialize destination settings with the PDF printer disabled.
        pdfPrinterDisabled = true;
        initialize();

        return nativeLayer.whenCalled('getPrinterCapabilities').then(() => {
          // Because the 'Save as PDF' fallback is unavailable, the first
          // destination is selected.
          assertDropdownItems([makeLocalDestinationKey('ID1')]);
        });
      });

  // Tests that disabling the 'Save as PDF' destination and exposing no
  // printers to the native layer results in a 'No destinations' option in the
  // dropdown.
  test(assert(destination_settings_test.TestNames.NoDestinations), function() {
    nativeLayer.setLocalDestinations([]);

    // Initialize destination settings with the PDF printer disabled.
    pdfPrinterDisabled = true;
    initialize();

    // 'getPrinters' will be called because there are no printers known to
    // the destination store and the 'Save as PDF' fallback is
    // unavailable.
    return nativeLayer.whenCalled('getPrinters').then(() => {
      assertDropdownItems(['noDestinations']);
    });
  });
});
