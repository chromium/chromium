// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://internet-detail-dialog/internet_detail_dialog_container.js';

import {InternetDetailDialogBrowserProxyImpl} from 'chrome://internet-detail-dialog/internet_detail_dialog_container.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

/** @implements {InternetDetailDialogBrowserProxy} */
export class TestInternetDetailDialogBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getDialogArguments',
      'dialogClose',
    ]);
  }

  /** @override */
  getDialogArguments() {
    return JSON.stringify({guid: 'guid'});
  }

  /** @override */
  dialogClose() {}
}

suite('internet-detail-dialog', () => {
  const guid = 'guid';
  const test_iccid = '11111111111111111';
  let internetDetailDialog = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function getManagedProperties(type, opt_source) {
    const result = OncMojo.getDefaultManagedProperties(type, guid, name);
    if (opt_source) {
      result.source = opt_source;
    }
    return result;
  }

  setup(async () => {
    PolymerTest.clearBody();
    InternetDetailDialogBrowserProxyImpl.instance_ =
        new TestInternetDetailDialogBrowserProxy();
    mojoApi_.resetForTest();
  });

  async function init() {
    internetDetailDialog = document.createElement('internet-detail-dialog');
    document.body.appendChild(internetDetailDialog);
    await flushAsync();
  }

  async function setupCellularNetwork(isPrimary, isInhibited) {
    const mojom = chromeos.networkConfig.mojom;
    await mojoApi_.setNetworkTypeEnabledState(
        mojom.NetworkType.kCellular, true);

    const cellularNetwork = getManagedProperties(
        mojom.NetworkType.kCellular, mojom.OncSource.kDevice);
    cellularNetwork.typeProperties.cellular.iccid = test_iccid;

    mojoApi_.setManagedPropertiesForTest(cellularNetwork);
    mojoApi_.setDeviceStateForTest({
      type: mojom.NetworkType.kCellular,
      deviceState: mojom.DeviceStateType.kEnabled,
      inhibitReason:
          (isInhibited ? mojom.InhibitReason.kInstallingProfile :
                         mojom.InhibitReason.kNotInhibited),
      simInfos: [{
        iccid: test_iccid,
        isPrimary: isPrimary,
      }],
    });
  }

  test('Network not on active sim, hide configurations', async () => {
    loadTimeData.overrideValues({
      updatedCellularActivationUi: true,
    });

    await setupCellularNetwork(/*isPrimary=*/ false, /*isInhibited=*/ false);

    await init();
    assertFalse(internetDetailDialog.showConfigurableSections_);

    const managedProperties = internetDetailDialog.managedProperties_;
    assertTrue(internetDetailDialog.showCellularSim_(managedProperties));
    assertFalse(!!internetDetailDialog.$$('network-siminfo'));

    assertFalse(!!internetDetailDialog.$$('cr-button'));
  });

  test('Network on active sim, show configurations', async () => {
    loadTimeData.overrideValues({
      updatedCellularActivationUi: true,
    });

    await setupCellularNetwork(/*isPrimary=*/ true, /*isInhibited=*/ false);

    await init();
    assertTrue(internetDetailDialog.showConfigurableSections_);

    const managedProperties = internetDetailDialog.managedProperties_;
    assertTrue(internetDetailDialog.showCellularSim_(managedProperties));
    assertTrue(!!internetDetailDialog.$$('network-siminfo'));

    assertTrue(!!internetDetailDialog.$$('cr-button'));
  });
});
