// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterfaceImpl, DestinationStore, LocalDestinationInfo, NativeLayerImpl, PrintPreviewUserManagerElement} from 'chrome://print/print_preview.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {CloudPrintInterfaceStub} from './cloud_print_interface_stub.js';

// <if expr="chromeos_ash or chromeos_lacros">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getCloudDestination, getDestinations, setupTestListenerElement} from './print_preview_test_utils.js';


suite('UserManagerTest', function() {
  let userManager: PrintPreviewUserManagerElement;

  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

  let cloudPrintInterface: CloudPrintInterfaceStub;

  const account1: string = 'foo@chromium.org';
  const account2: string = 'bar@chromium.org';

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    document.body.innerHTML = '';

    // Create data classes
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="chromeos_ash or chromeos_lacros">
    setNativeLayerCrosInstance();
    // </if>
    cloudPrintInterface = new CloudPrintInterfaceStub();
    CloudPrintInterfaceImpl.setInstance(cloudPrintInterface);
    cloudPrintInterface.setPrinter(
        getCloudDestination('FooCloud', 'FooCloudName', account1));

    userManager = document.createElement('print-preview-user-manager');

    // Initialize destination store.
    destinationStore = createDestinationStore();
    destinationStore.setCloudPrintInterface(cloudPrintInterface);
    const localDestinations: LocalDestinationInfo[] = [];
    getDestinations(localDestinations);
    nativeLayer.setLocalDestinations(localDestinations);

    // Set up user manager
    userManager.destinationStore = destinationStore;
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
          cloudPrintInterface.setPrinter(
              getCloudDestination('BarCloud', 'BarCloudName', account2));
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
