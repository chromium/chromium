// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/psim_flow_ui.m.js';

// #import {PSimUIState} from 'chrome://resources/cr_components/chromeos/cellular_setup/psim_flow_ui.m.js';
// #import {setCellularSetupRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.m.js';
// #import {FakeCarrierPortalHandlerRemote, FakeCellularSetupRemote} from './fake_cellular_setup_remote.m.js';
// clang-format on

suite('CrComponentsPsimFlowUiTest', function() {
  let pSimPage;

  /** @type {?chromeos.cellularSetup.mojom.CellularSetupRemote} */
  let cellularSetupRemote = null;

  /** @type {?FakeCarrierPortalHandlerRemote} */
  let cellularCarrierHandler = null;

  /** @type {?chromeos.cellularSetup.mojom.ActivationDelegateReceiver} */
  let cellularActivationDelegate = null;

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  suiteSetup(function() {
    cellularCarrierHandler =
        new cellular_setup.FakeCarrierPortalHandlerRemote();
    cellularSetupRemote =
        new cellular_setup.FakeCellularSetupRemote(cellularCarrierHandler);
    cellular_setup.setCellularSetupRemoteForTesting(cellularSetupRemote);
  });

  setup(function() {
    pSimPage = document.createElement('psim-flow-ui');
    pSimPage.delegate = new cellular_setup.FakeCellularSetupDelegate();
    pSimPage.initSubflow();
    document.body.appendChild(pSimPage);
    Polymer.dom.flush();
  });

  test('Base test', function() {
    const ironPage = pSimPage.$$('iron-pages');
    assertTrue(!!ironPage);
  });

  test('forward navigation test', function() {
    pSimPage.state_ = cellularSetup.PSimUIState.WAITING_FOR_PORTAL_TO_LOAD;
    Polymer.dom.flush();
    pSimPage.navigateForward();
    assertTrue(
        pSimPage.state_ ===
        cellularSetup.PSimUIState.WAITING_FOR_ACTIVATION_TO_FINISH);
  });

  test('Carrier title on provisioning page', async () => {
    cellularActivationDelegate =
        cellularSetupRemote.getLastActivationDelegate();

    cellularActivationDelegate.onActivationStarted({
      paymentUrl: {url: ''},
      paymentPostData: 'verison_post_data',
      carrier: 'Verizon wireless',
      meid: '012345678912345',
      imei: '012345678912345',
      mdn: '0123456789'
    });

    cellularCarrierHandler.onCarrierPortalStatusChange(
        chromeos.cellularSetup.mojom.CarrierPortalStatus
            .kPortalLoadedWithoutPaidUser);

    await flushAsync();

    assertTrue(pSimPage.nameOfCarrierPendingSetup === 'Verizon wireless');
  });
});
