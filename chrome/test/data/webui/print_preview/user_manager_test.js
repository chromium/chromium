// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterfaceImpl, DestinationStore, NativeLayer, NativeLayerImpl} from 'chrome://print/print_preview.js';
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
    cloudPrintInterface.setPrinter(getGoogleDriveDestination(account1));

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
    userManager.shouldReloadCookies = false;
    userManager.cloudPrintDisabled = false;
    document.body.appendChild(userManager);

    destinationStore.init(
        false /* pdfPrinterDisabled */, true /* isDriveMounted */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */, []);
  });

  // Checks that initializing and updating user accounts works as expected.
  test('update users without sync', function() {
    userManager.initUserAccounts();
    assertEquals(undefined, userManager.activeUser);
    assertEquals(0, userManager.users.length);
    assertEquals(0, cloudPrintInterface.getCallCount('search'));

    // Simulate triggering the dialog since we have set a cloud printer. This
    // should update the list of users and the active user and trigger a call to
    // search.
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
