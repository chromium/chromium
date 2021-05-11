// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterfaceEventType, CloudPrintInterfaceImpl, DestinationStore, InvitationStore, NativeLayer, NativeLayerImpl} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {CloudPrintInterfaceStub} from 'chrome://test/print_preview/cloud_print_interface_stub.js';
import {NativeLayerStub} from 'chrome://test/print_preview/native_layer_stub.js';
import {createDestinationStore, getDestinations, getGoogleDriveDestination, setupTestListenerElement} from 'chrome://test/print_preview/print_preview_test_utils.js';

import {eventToPromise} from '../test_util.m.js';

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
    cloudPrintInterface.setPrinter(getGoogleDriveDestination(account1));
    const whenSearchDone = eventToPromise(
        CloudPrintInterfaceEventType.SEARCH_DONE,
        cloudPrintInterface.getEventTarget());

    userManager = document.createElement('print-preview-user-manager');

    // Initialize destination store.
    destinationStore = createDestinationStore();
    destinationStore.setCloudPrintInterface(cloudPrintInterface);
    const localDestinations = [];
    const destinations = getDestinations(localDestinations);
    nativeLayer.setLocalDestinations(localDestinations);

    // Set up user manager
    userManager.appKioskMode = false;
    userManager.destinationStore = destinationStore;
    userManager.invitationStore = new InvitationStore();
    userManager.shouldReloadCookies = false;
    userManager.cloudPrintDisabled = false;
    document.body.appendChild(userManager);

    destinationStore.init(
        false /* pdfPrinterDisabled */, true /* isDriveMounted */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */, []);

    // <if expr="not chromeos">
    return whenSearchDone;
    // </if>
  });

  // Checks that initializing and updating user accounts works as expected.
  test('update users without sync', function() {
    // Destination store will trigger a call to cloud print to determine the
    // state of the Drive printer, which will in turn set the account state.
    // This doesn't happen on Chrome OS which uses a different Drive.
    userManager.initUserAccounts();
    assertEquals(isChromeOS ? undefined : account1, userManager.activeUser);
    assertEquals(isChromeOS ? 0 : 1, userManager.users.length);
    assertEquals(
        isChromeOS ? 0 : 2, cloudPrintInterface.getCallCount('search'));

    // Start reloading everything. This will set up the account on Chrome OS,
    // which doesn't use the Google Cloud Print Drive in the dropdown.
    destinationStore.startLoadAllDestinations();
    return cloudPrintInterface.whenCalled('search')
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
});
