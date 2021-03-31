// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {routes, Router} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertTrue} from '../../chai_assert.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';
// clang-format on

suite('InternetDetailMenu', function() {
  let internetDetailMenu;
  let mojoApi_;
  let mojom;

  setup(function() {
    loadTimeData.overrideValues({
      updatedCellularActivationUi: true,
    });
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    mojoApi_.resetForTest();

    mojom = chromeos.networkConfig.mojom;
    mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, true);
  });

  function getManagedProperties(type, name) {
    const result =
        OncMojo.getDefaultManagedProperties(type, name + '_guid', name);
    return result;
  }

  async function init() {
    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    internetDetailMenu =
        document.createElement('settings-internet-detail-menu');

    document.body.appendChild(internetDetailMenu);
    assertTrue(!!internetDetailMenu);
    await flushAsync();
  }

  async function addEsimCellularNetwork(iccid, eid) {
    const cellular =
        getManagedProperties(mojom.NetworkType.kCellular, 'cellular');
    cellular.typeProperties.cellular.iccid = iccid;
    cellular.typeProperties.cellular.eid = eid;
    mojoApi_.setManagedPropertiesForTest(cellular);
    await flushAsync();
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Do not show tripple dot when no iccid is present', async function() {
    addEsimCellularNetwork(null, '11111111111111111111111111111111');
    await init();

    let trippleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertFalse(!!trippleDot);

    addEsimCellularNetwork('100000', '11111111111111111111111111111111');

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    trippleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!trippleDot);
  });

  test('Do not show tripple dot when no eid is present', async function() {
    addEsimCellularNetwork('100000', null);
    await init();

    let trippleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertFalse(!!trippleDot);

    addEsimCellularNetwork('100000', '11111111111111111111111111111111');

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    trippleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!trippleDot);
  });

  test('Rename menu click', async function() {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    const trippleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!trippleDot);

    trippleDot.click();
    await flushAsync();

    const renameBtn =
        internetDetailMenu.shadowRoot.querySelector('cr-action-menu')
            .querySelector('#renameBtn');
    assertTrue(!!renameBtn);

    const renameProfilePromise = test_util.eventToPromise(
        'show-esim-profile-rename-dialog', internetDetailMenu);
    renameBtn.click();
    await Promise.all([renameProfilePromise, test_util.flushTasks()]);
  });

  test('Remove menu button click', async function() {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    const trippleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!trippleDot);

    trippleDot.click();
    await flushAsync();
    const removeBtn =
        internetDetailMenu.shadowRoot.querySelector('cr-action-menu')
            .querySelector('#removeBtn');
    assertTrue(!!removeBtn);

    const removeProfilePromise = test_util.eventToPromise(
        'show-esim-remove-profile-dialog', internetDetailMenu);
    removeBtn.click();
    await Promise.all([removeProfilePromise, test_util.flushTasks()]);
  });

  test('Menu is disabled when inhibited', async function() {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    init();

    const params = new URLSearchParams;
    params.append('guid', 'cellular_guid');
    settings.Router.getInstance().navigateTo(
        settings.routes.NETWORK_DETAIL, params);

    await flushAsync();
    const trippleDot = internetDetailMenu.$$('#moreNetworkDetail');
    assertTrue(!!trippleDot);
    assertFalse(trippleDot.disabled);

    internetDetailMenu.deviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      inhibitReason: mojom.InhibitReason.kConnectingToProfile,
    };
    assertTrue(trippleDot.disabled);

    internetDetailMenu.deviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      inhibitReason: mojom.InhibitReason.kNotInhibited,
    };
    assertFalse(trippleDot.disabled);
  });
});