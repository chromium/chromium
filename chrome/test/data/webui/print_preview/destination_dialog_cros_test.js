// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationStore, DestinationType, LocalDestinationInfo, makeRecentDestination, NativeLayerImpl, RecentDestination} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise, waitAfterNextRender} from '../test_util.m.js';

import {CloudPrintInterfaceStub} from './cloud_print_interface_stub.js';
import {NativeLayerCrosStub, setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getDestinations, getGoogleDriveDestination, setupTestListenerElement} from './print_preview_test_utils.js';

window.destination_dialog_cros_test = {};
const destination_dialog_cros_test = window.destination_dialog_cros_test;
destination_dialog_cros_test.suiteName = 'DestinationDialogCrosTest';
/** @enum {string} */
destination_dialog_cros_test.TestNames = {
  PrinterList: 'PrinterList',
  ShowProvisionalDialog: 'ShowProvisionalDialog',
  UserAccounts: 'UserAccounts',
  PrintServersChanged: 'PrintServersChanged',
  PrintServerSelected: 'PrintServerSelected',
};

suite(destination_dialog_cros_test.suiteName, function() {
  /** @type {!PrintPreviewDestinationDialogCrosElement} */
  let dialog;

  /** @type {!DestinationStore} */
  let destinationStore;

  /** @type {!NativeLayerStub} */
  let nativeLayer;

  /** @type {!NativeLayerCrosStub} */
  let nativeLayerCros;

  /** @type {!CloudPrintInterfaceStub} */
  let cloudPrintInterface;

  /** @type {!Array<!Destination>} */
  let destinations = [];

  /** @type {!Array<!LocalDestinationInfo>} */
  const localDestinations = [];

  /** @type {!Array<!RecentDestination>} */
  let recentDestinations = [];

  /** @override */
  suiteSetup(function() {
    setupTestListenerElement();
  });

  /** @override */
  setup(function() {
    // Create data classes
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    nativeLayerCros = setNativeLayerCrosInstance();
    cloudPrintInterface = new CloudPrintInterfaceStub();
    destinationStore = createDestinationStore();
    destinationStore.setCloudPrintInterface(cloudPrintInterface);
    destinations = getDestinations(localDestinations);
    recentDestinations = [makeRecentDestination(destinations[4])];
    nativeLayer.setLocalDestinations(localDestinations);
    destinationStore.init(
        false /* pdfPrinterDisabled */, true /* isDriveMounted */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        recentDestinations /* recentDestinations */);

    // Set up dialog
    dialog = /** @type {!PrintPreviewDestinationDialogCrosElement} */ (
        document.createElement('print-preview-destination-dialog-cros'));
    dialog.activeUser = '';
    dialog.users = [];
    dialog.destinationStore = destinationStore;
  });

  function finishSetup() {
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
  test(assert(destination_dialog_cros_test.TestNames.PrinterList), async () => {
    await finishSetup();
    const list = dialog.$$('print-preview-destination-list');

    const printerItems =
        list.shadowRoot.querySelectorAll('print-preview-destination-list-item');

    const getDisplayedName = item => item.$$('.name').textContent;
    // 5 printers + Save as PDF
    assertEquals(6, printerItems.length);
    // Save as PDF shows up first.
    assertEquals(
        Destination.GooglePromotedId.SAVE_AS_PDF,
        getDisplayedName(printerItems[0]));
    assertEquals(
        'rgb(32, 33, 36)',
        window.getComputedStyle(printerItems[0].$$('.name')).color);
    Array.from(printerItems).slice(1, 5).forEach((item, index) => {
      assertEquals(destinations[index].displayName, getDisplayedName(item));
    });
    assertEquals('FooName', getDisplayedName(printerItems[5]));
  });

  // Test that clicking a provisional destination shows the provisional
  // destinations dialog, and that the escape key closes only the provisional
  // dialog when it is open, not the destinations dialog.
  test(
      assert(destination_dialog_cros_test.TestNames.ShowProvisionalDialog),
      async () => {
        let provisionalDialog = null;
        const provisionalDestination = {
          extensionId: 'ABC123',
          extensionName: 'ABC Printing',
          id: 'XYZDevice',
          name: 'XYZ',
          provisional: true,
        };

        // Set the extension destinations and force the destination store to
        // reload printers.
        nativeLayer.setExtensionDestinations([provisionalDestination]);
        await finishSetup();
        flush();
        provisionalDialog =
            dialog.$$('print-preview-provisional-destination-resolver');
        assertFalse(provisionalDialog.$$('#dialog').open);
        const list = dialog.$$('print-preview-destination-list');
        const printerItems = list.shadowRoot.querySelectorAll(
            'print-preview-destination-list-item');

        // Should have 5 local destinations, Save as PDF + extension
        // destination.
        assertEquals(7, printerItems.length);
        const provisionalItem = Array.from(printerItems).find(printerItem => {
          return printerItem.destination.id === provisionalDestination.id;
        });

        // Click the provisional destination to select it.
        provisionalItem.click();
        flush();
        assertTrue(provisionalDialog.$$('#dialog').open);

        // Send escape key on provisionalDialog. Destinations dialog should
        // not close.
        const whenClosed = eventToPromise('close', provisionalDialog);
        keyEventOn(provisionalDialog, 'keydown', 19, [], 'Escape');
        flush();
        await whenClosed;

        assertFalse(provisionalDialog.$$('#dialog').open);
        assertTrue(dialog.$$('#dialog').open);
      });

  /**
   * @param {string} account The current active user account.
   * @param {number} numUsers The total number of users that are signed in.
   */
  function assertSignedInState(account, numUsers) {
    const signedIn = account !== '';
    assertEquals(!signedIn, dialog.$$('.user-info').hidden);

    if (numUsers > 0) {
      const userSelect = dialog.$$('.md-select');
      const userSelectOptions = userSelect.querySelectorAll('option');
      assertEquals(numUsers + 1, userSelectOptions.length);
      assertEquals('', userSelectOptions[numUsers].value);
      assertEquals(account, userSelect.value);
    }
  }

  /** @param {number} numPrinters The total number of available printers. */
  function assertNumPrintersVisible(numPrinters) {
    const list = dialog.$$('print-preview-destination-list');
    const printerItems = list.shadowRoot.querySelectorAll(
        'print-preview-destination-list-item:not([hidden])');
    assertEquals(numPrinters, printerItems.length);
  }

  // Test that signing in and switching accounts works as expected.
  test(
      assert(destination_dialog_cros_test.TestNames.UserAccounts), async () => {
        // Set up the cloud print interface with Google Drive printer for a
        // couple different accounts.
        const user1 = 'foo@chromium.org';
        const user2 = 'bar@chromium.org';
        cloudPrintInterface.setPrinter(getGoogleDriveDestination(user1));
        cloudPrintInterface.setPrinter(getGoogleDriveDestination(user2));
        // Override so that privet printers will also be fetched, since we are
        // simulating the case where the enterprise override is enabled.
        loadTimeData.overrideValues({'forceEnablePrivetPrinting': true});
        let userSelect = null;

        await finishSetup();
        // Check that the user dropdown is hidden when there are no active
        // users.
        assertTrue(dialog.$$('.user-info').hidden);
        userSelect = dialog.$$('.md-select');

        // Enable cloud print.
        assertSignedInState('', 0);
        // Local, extension, privet, and cloud (since
        // startLoadAllDestinations() was called).
        assertEquals(3, nativeLayer.getCallCount('getPrinters'));
        assertEquals(1, cloudPrintInterface.getCallCount('search'));

        // 6 printers, no Google drive (since not signed in).
        assertNumPrintersVisible(6);

        // Set an active user.
        destinationStore.setActiveUser(user1);
        destinationStore.reloadUserCookieBasedDestinations(user1);
        dialog.activeUser = user1;
        dialog.users = [user1];
        flush();

        // Select shows the signed in user.
        assertSignedInState(user1, 1);

        const expectedPrinters = 6;
        assertNumPrintersVisible(expectedPrinters);
        assertEquals(3, nativeLayer.getCallCount('getPrinters'));
        // Cloud printers should have been re-fetched.
        assertEquals(2, cloudPrintInterface.getCallCount('search'));

        // Simulate signing into a second account.
        userSelect.value = '';
        userSelect.dispatchEvent(new CustomEvent('change'));

        await nativeLayer.whenCalled('signIn');
        // No new printer fetch until the user actually changes the active
        // account.
        assertEquals(3, nativeLayer.getCallCount('getPrinters'));
        assertEquals(2, cloudPrintInterface.getCallCount('search'));
        dialog.users = [user1, user2];
        flush();

        // Select shows the signed in user.
        assertSignedInState(user1, 2);

        // Still have 6 printers, with user1 signed in.
        assertNumPrintersVisible(expectedPrinters);

        // Select the second account.
        const whenEventFired = eventToPromise('account-change', dialog);
        userSelect.value = user2;
        userSelect.dispatchEvent(new CustomEvent('change'));

        await whenEventFired;
        flush();

        // This will all be done by app.js and user_manager.js in response
        // to the account-change event.
        destinationStore.setActiveUser(user2);
        dialog.activeUser = user2;
        const whenInserted = eventToPromise(
            DestinationStore.EventType.DESTINATIONS_INSERTED, destinationStore);
        destinationStore.reloadUserCookieBasedDestinations(user2);

        await whenInserted;
        flush();

        assertSignedInState(user2, 2);

        // 6 printers, with user2 signed in.
        assertNumPrintersVisible(expectedPrinters);
        assertEquals(3, nativeLayer.getCallCount('getPrinters'));
        // Cloud print should have been queried again for the new account.
        assertEquals(3, cloudPrintInterface.getCallCount('search'));
      });

  // Test that checks that print server searchable input and its selections are
  // updated according to the PRINT_SERVERS_CHANGED event.
  test(
      assert(destination_dialog_cros_test.TestNames.PrintServersChanged),
      async () => {
        await finishSetup();

        const printServers = [
          {id: 'print-server-1', name: 'Print Server 1'},
          {id: 'print-server-2', name: 'Print Server 2'},
        ];
        const isSingleServerFetchingMode = true;
        webUIListenerCallback('print-servers-config-changed', {
          printServers: printServers,
          isSingleServerFetchingMode: isSingleServerFetchingMode,
        });
        await waitAfterNextRender(dialog);

        assertFalse(dialog.$$('.server-search-box-input').hidden);
        const serverSelector = dialog.$$('.server-search-box-input');
        const serverSelections =
            serverSelector.shadowRoot.querySelectorAll('.list-item');
        assertEquals('Print Server 1', serverSelections[0].textContent.trim());
        assertEquals('Print Server 2', serverSelections[1].textContent.trim());
      });

  // Tests that choosePrintServers is called when the print server searchable
  // input value is changed.
  test(
      assert(destination_dialog_cros_test.TestNames.PrintServerSelected),
      async () => {
        await finishSetup();
        const printServers = [
          {id: 'user-print-server-1', name: 'Print Server 1'},
          {id: 'user-print-server-2', name: 'Print Server 2'},
          {id: 'device-print-server-1', name: 'Print Server 1'},
          {id: 'device-print-server-2', name: 'Print Server 2'},
        ];
        const isSingleServerFetchingMode = true;
        webUIListenerCallback('print-servers-config-changed', {
          printServers: printServers,
          isSingleServerFetchingMode: isSingleServerFetchingMode,
        });
        await waitAfterNextRender(dialog);
        nativeLayerCros.reset();

        const pendingPrintServerId =
            nativeLayerCros.whenCalled('choosePrintServers');
        dialog.$$('cr-searchable-drop-down').value = 'Print Server 2';
        await waitAfterNextRender(dialog);

        assertEquals(1, nativeLayerCros.getCallCount('choosePrintServers'));
        assertDeepEquals(
            ['user-print-server-2', 'device-print-server-2'],
            await pendingPrintServerId);
      });
});
