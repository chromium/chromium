// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterfaceImpl, DestinationStore, InvitationStore, NativeLayer, NativeLayerImpl} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {CloudPrintInterfaceStub} from 'chrome://test/print_preview/cloud_print_interface_stub.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {createDestinationStore, getDestinations, getGoogleDriveDestination, setupTestListenerElement} from 'chrome://test/print_preview/print_preview_test_utils.js';

// <if expr="chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

suite('UserManagerTest', function() {
  /** @type {?PrintPreviewUserManagerElement} */
  let userManager = null;

  /** @type {?DestinationStore} */
  let destinationStore = null;

  /** @type {?NativeLayer} */
  let nativeLayer = null;

  /** @type {?CloudPrintInterface} */
  let cloudPrintInterface = null;

  const account1 = 'foo@chromium.org';
  const account2 = 'bar@chromium.org';

  /** @override */
  suiteSetup(function() {
    setupTestListenerElement();
  });

  /** @override */
  setup(function() {
    document.body.innerHTML = '';

    // Create data classes
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    // <if expr="chromeos">
    setNativeLayerCrosInstance();
    // </if>
    cloudPrintInterface = new CloudPrintInterfaceStub();
    CloudPrintInterfaceImpl.instance_ = cloudPrintInterface;

    userManager = document.createElement('print-preview-user-manager');

    // Initialize destination store.
    destinationStore = createDestinationStore();
    destinationStore.setCloudPrintInterface(cloudPrintInterface);
    const localDestinations = [];
    const destinations = getDestinations(localDestinations);
    nativeLayer.setLocalDestinations(localDestinations);
    destinationStore.init(
        false /* pdfPrinterDisabled */, true /* isDriveMounted */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */, []);

    // Set up user manager
    userManager.appKioskMode = false;
    userManager.destinationStore = destinationStore;
    userManager.invitationStore = new InvitationStore();
    userManager.shouldReloadCookies = false;
    document.body.appendChild(userManager);
  });

  // Checks that initializing and updating user accounts works as expected.
  test('update users', function() {
    // Set up a cloud printer for each account.
    cloudPrintInterface.setPrinter(getGoogleDriveDestination(account1));
    cloudPrintInterface.setPrinter(getGoogleDriveDestination(account2));

    assertEquals(undefined, userManager.activeUser);

    userManager.cloudPrintDisabled = false;
    userManager.initUserAccounts([], true /* syncAvailable */);
    assertEquals('', userManager.activeUser);
    assertEquals(0, userManager.users.length);
    assertEquals(0, cloudPrintInterface.getCallCount('search'));

    // Simulate signing in and out of accounts. This should update the list of
    // users and the active user, and should refresh the list of cloud printers
    // so that we can confirm Google Drive is available.
    webUIListenerCallback('user-accounts-updated', [account1]);
    assertEquals(account1, userManager.activeUser);
    assertEquals(1, userManager.users.length);
    assertEquals(1, cloudPrintInterface.getCallCount('search'));

    webUIListenerCallback('user-accounts-updated', [account1, account2]);
    assertEquals(account1, userManager.activeUser);
    assertEquals(2, userManager.users.length);
    // Still 1 search since the active user didn't change.
    assertEquals(1, cloudPrintInterface.getCallCount('search'));

    webUIListenerCallback('user-accounts-updated', [account2]);
    assertEquals(account2, userManager.activeUser);
    assertEquals(1, userManager.users.length);
    assertEquals(2, cloudPrintInterface.getCallCount('search'));

    webUIListenerCallback('user-accounts-updated', []);
    assertEquals('', userManager.activeUser);
    assertEquals(0, userManager.users.length);
    assertEquals(2, cloudPrintInterface.getCallCount('search'));
  });

  // Checks that initializing and updating user accounts works as expected
  // when sync is unavailable.
  test('update users without sync', function() {
    const whenCalled = cloudPrintInterface.whenCalled('search');
    assertEquals(undefined, userManager.activeUser);

    userManager.cloudPrintDisabled = false;
    userManager.initUserAccounts([], false /* syncAvailable */);
    return whenCalled
        .then(() => {
          assertEquals(undefined, userManager.activeUser);
          assertEquals(0, userManager.users.length);
          assertEquals(1, cloudPrintInterface.getCallCount('search'));
          cloudPrintInterface.resetResolver('search');

          // Simulate signing into an account by setting a cloud printer for
          // it and firing the 'check-for-account-update' listener.
          // This should update the list of users and the active user and
          // trigger a call to search.
          cloudPrintInterface.setPrinter(getGoogleDriveDestination(account1));
          webUIListenerCallback('check-for-account-update');
          return cloudPrintInterface.whenCalled('search');
        })
        .then(() => {
          assertEquals(account1, userManager.activeUser);
          assertEquals(1, userManager.users.length);
          assertEquals(2, cloudPrintInterface.getCallCount('search'));

          // Simulate signing in to a second account.
          cloudPrintInterface.setPrinter(getGoogleDriveDestination(account2));
          cloudPrintInterface.resetResolver('search');
          webUIListenerCallback('check-for-account-update');
          return cloudPrintInterface.whenCalled('search');
        })
        .then(() => {
          assertEquals(account1, userManager.activeUser);
          assertEquals(2, userManager.users.length);
          assertEquals(1, cloudPrintInterface.getCallCount('search'));
        });
  });

  test('update active user', function() {
    // Set up a cloud printer for each account.
    cloudPrintInterface.setPrinter(getGoogleDriveDestination(account1));
    cloudPrintInterface.setPrinter(getGoogleDriveDestination(account2));
    userManager.cloudPrintDisabled = false;
    userManager.initUserAccounts(
        [account1, account2], true /* syncAvailable */);
    assertEquals(account1, userManager.activeUser);
    assertEquals(2, userManager.users.length);

    // Search is called once at startup.
    return cloudPrintInterface.whenCalled('search')
        .then(() => {
          assertEquals(1, cloudPrintInterface.getCallCount('search'));

          // Changing the active user results in a new search call.
          cloudPrintInterface.resetResolver('search');
          const whenSearchCalled = cloudPrintInterface.whenCalled('search');
          userManager.updateActiveUser(account2);
          assertEquals(account2, userManager.activeUser);
          assertEquals(2, userManager.users.length);
          return whenSearchCalled;
        })
        .then(() => {
          assertEquals(1, cloudPrintInterface.getCallCount('search'));

          // No new search call when switching back.
          userManager.shouldReloadCookies = true;
          userManager.updateActiveUser(account1);
          assertEquals(account1, userManager.activeUser);
          assertEquals(2, userManager.users.length);
          assertEquals(1, cloudPrintInterface.getCallCount('search'));
        });
  });
});
