// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationStore, DestinationStoreEventType, GooglePromotedDestinationId, LocalDestinationInfo, makeRecentDestination, NativeLayerImpl, PrintPreviewDestinationDialogElement, PrintPreviewDestinationListItemElement, RecentDestination} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {CloudPrintInterfaceStub} from './cloud_print_interface_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getDestinations, getGoogleDriveDestination, setupTestListenerElement} from './print_preview_test_utils.js';

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

  let cloudPrintInterface: CloudPrintInterfaceStub;

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
    cloudPrintInterface = new CloudPrintInterfaceStub();
    destinationStore = createDestinationStore();
    destinationStore.setCloudPrintInterface(cloudPrintInterface);
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
    dialog.activeUser = '';
    dialog.users = [];
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

  /**
   * @param account The current active user account.
   * @param numUsers The total number of users that are signed in.
   */
  function assertSignedInState(account: string, numUsers: number) {
    const signedIn = account !== '';
    assertEquals(
        !signedIn,
        dialog.shadowRoot!.querySelector<HTMLElement>('.user-info')!.hidden);

    if (numUsers > 0) {
      const userSelect =
          dialog.shadowRoot!.querySelector<HTMLSelectElement>('.md-select')!;
      const userSelectOptions =
          userSelect.querySelectorAll<HTMLOptionElement>('option');
      assertEquals(numUsers + 1, userSelectOptions.length);
      assertEquals('', userSelectOptions[numUsers]!.value);
      assertEquals(account, userSelect.value);
    }
  }

  /**
   * @param numPrinters The total number of available printers.
   * @param account The current active user account.
   */
  function assertNumPrintersWithDriveAccount(
      numPrinters: number, account: string) {
    const list =
        dialog.shadowRoot!.querySelector('print-preview-destination-list')!;
    const printerItems =
        list.shadowRoot!
            .querySelectorAll<PrintPreviewDestinationListItemElement>(
                'print-preview-destination-list-item:not([hidden])');
    assertEquals(numPrinters, printerItems.length);

    const drivePrinter = Array.from(printerItems).find(item => {
      return item.destination.id === GooglePromotedDestinationId.DOCS;
    });
    assertEquals(!!drivePrinter, account !== '');
    if (drivePrinter) {
      assertEquals(account, drivePrinter.destination.account);
    }
  }

  // Test that signing in and switching accounts works as expected.
  test(assert(destination_dialog_test.TestNames.UserAccounts), async () => {
    // Set up the cloud print interface with Google Drive printer for a couple
    // different accounts.
    const user1 = 'foo@chromium.org';
    const user2 = 'bar@chromium.org';
    cloudPrintInterface.setPrinter(getGoogleDriveDestination(user1));
    cloudPrintInterface.setPrinter(getGoogleDriveDestination(user2));

    await finishSetup();
    // Check that the user dropdown is hidden when there are no active users.
    assertTrue(
        dialog.shadowRoot!.querySelector<HTMLElement>('.user-info')!.hidden);
    const userSelect =
        dialog.shadowRoot!.querySelector<HTMLSelectElement>('.md-select')!;

    // Enable cloud print.
    assertSignedInState('', 0);
    // Local, extension, and cloud (since
    // startLoadAllDestinations() was called).
    assertEquals(2, nativeLayer.getCallCount('getPrinters'));
    assertEquals(1, cloudPrintInterface.getCallCount('search'));

    // 6 printers, no Google drive (since not signed in).
    assertNumPrintersWithDriveAccount(6, '');

    // Set an active user.
    destinationStore.setActiveUser(user1);
    destinationStore.reloadUserCookieBasedDestinations(user1);
    dialog.activeUser = user1;
    dialog.users = [user1];
    flush();

    // Select shows the signed in user.
    assertSignedInState(user1, 1);

    // Now have 7 printers (Google Drive), with user1 signed in.
    const expectedPrinters = 7;
    assertNumPrintersWithDriveAccount(expectedPrinters, user1);
    // Still 2 calls as extension and local printers don't get refreshed on
    // sign in.
    assertEquals(2, nativeLayer.getCallCount('getPrinters'));
    // Cloud printers should have been re-fetched.
    assertEquals(2, cloudPrintInterface.getCallCount('search'));

    // Simulate signing into a second account.
    userSelect.value = '';
    userSelect.dispatchEvent(new CustomEvent('change'));

    await nativeLayer.whenCalled('signIn');
    // No new printer fetch until the user actually changes the active
    // account.
    assertEquals(2, nativeLayer.getCallCount('getPrinters'));
    assertEquals(2, cloudPrintInterface.getCallCount('search'));
    dialog.users = [user1, user2];
    flush();

    // Select shows the signed in user.
    assertSignedInState(user1, 2);

    // Still have 7 printers (Google Drive), with user1 signed in.
    assertNumPrintersWithDriveAccount(expectedPrinters, user1);

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
        DestinationStoreEventType.DESTINATIONS_INSERTED, destinationStore);
    destinationStore.reloadUserCookieBasedDestinations(user2);

    await whenInserted;
    flush();

    assertSignedInState(user2, 2);

    // 7 printers (Google Drive), with user2 signed in.
    assertNumPrintersWithDriveAccount(expectedPrinters, user2);
    assertEquals(2, nativeLayer.getCallCount('getPrinters'));
    // Cloud print should have been queried again for the new account.
    assertEquals(3, cloudPrintInterface.getCallCount('search'));
  });
});
