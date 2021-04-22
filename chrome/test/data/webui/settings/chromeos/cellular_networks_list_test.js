// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {MultiDeviceFeatureState, MultiDeviceBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.m.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';
// #import {CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// clang-format on

suite('CellularNetworksList', function() {
  let cellularNetworkList;

  let mojom;

  /** @type {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote|undefined} */
  let mojoApi_;

  let eSimManagerRemote;
  let browserProxy;

  setup(function() {
    mojom = chromeos.networkConfig.mojom;
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;

    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);

    browserProxy = new multidevice.TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;
  });

  teardown(function() {
    cellularNetworkList.remove();
    cellularNetworkList = null;
  });

  function init() {
    cellularNetworkList = document.createElement('cellular-networks-list');
    // iron-list will not create list items if the container of the list is of
    // size zero.
    cellularNetworkList.style.height = '100%';
    cellularNetworkList.style.width = '100%';
    document.body.appendChild(cellularNetworkList);
    Polymer.dom.flush();
  }

  function setManagedPropertiesForTest(type, properties) {
    mojoApi_.resetForTest();
    mojoApi_.setNetworkTypeEnabledState(type, true);
    const networks = [];
    for (let i = 0; i < properties.length; i++) {
      mojoApi_.setManagedPropertiesForTest(properties[i]);
      networks.push(OncMojo.managedPropertiesToNetworkState(properties[i]));
    }
    cellularNetworkList.cellularDeviceState =
        mojoApi_.getDeviceStateForTest(type);
    cellularNetworkList.networks = networks;
  }

  function initSimInfos() {
    let deviceState = cellularNetworkList.cellularDeviceState;
    if (!deviceState) {
      deviceState = {
        type: mojom.NetworkType.kCellular,
        deviceState: mojom.DeviceStateType.kEnabled,
        inhibitReason: mojom.InhibitReason.kNotInhibited,
      };
    }
    if (!deviceState.simInfos) {
      deviceState.simInfos = [];
    }
    cellularNetworkList.cellularDeviceState = deviceState;
  }

  function addPSimSlot() {
    initSimInfos();
    // Make a copy so observers get fired.
    const simInfos = [...cellularNetworkList.cellularDeviceState.simInfos];
    simInfos.push({
      iccid: '',
    });
    cellularNetworkList.set('cellularDeviceState.simInfos', simInfos);
    return flushAsync();
  }

  function addESimSlot() {
    initSimInfos();
    // Make a copy so observers get fired.
    const simInfos = [...cellularNetworkList.cellularDeviceState.simInfos];
    simInfos.push({
      eid: 'eid',
    });
    cellularNetworkList.set('cellularDeviceState.simInfos', simInfos);
    return flushAsync();
  }

  function clearSimSlots() {
    cellularNetworkList.set('cellularDeviceState.simInfos', []);
    return flushAsync();
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Tether, cellular and eSIM profiles', async () => {
    eSimManagerRemote.addEuiccForTest(2);
    init();
    browserProxy.setInstantTetheringStateForTest(
        settings.MultiDeviceFeatureState.ENABLED_BY_USER);

    const eSimNetwork1 = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular_esim1');
    eSimNetwork1.typeProperties.cellular.eid =
        '11111111111111111111111111111111';
    const eSimNetwork2 = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular_esim2');
    eSimNetwork2.typeProperties.cellular.eid =
        '22222222222222222222222222222222';
    setManagedPropertiesForTest(mojom.NetworkType.kCellular, [
      OncMojo.getDefaultManagedProperties(
          mojom.NetworkType.kCellular, 'cellular1'),
      OncMojo.getDefaultManagedProperties(
          mojom.NetworkType.kCellular, 'cellular2'),
      eSimNetwork1,
      eSimNetwork2,
      OncMojo.getDefaultManagedProperties(mojom.NetworkType.kTether, 'tether1'),
      OncMojo.getDefaultManagedProperties(mojom.NetworkType.kTether, 'tether2'),
    ]);
    addPSimSlot();
    addESimSlot();

    await flushAsync();

    const eSimNetworkList = cellularNetworkList.$$('#esimNetworkList');
    assertTrue(!!eSimNetworkList);

    const pSimNetworkList = cellularNetworkList.$$('#psimNetworkList');
    assertTrue(!!pSimNetworkList);

    const tetherNetworkList = cellularNetworkList.$$('#tetherNetworkList');
    assertTrue(!!tetherNetworkList);

    assertEquals(2, eSimNetworkList.networks.length);
    assertEquals(2, pSimNetworkList.networks.length);
    assertEquals(2, tetherNetworkList.networks.length);
    assertEquals(2, eSimNetworkList.customItems.length);
  });

  test(
      'Fire show cellular setup event on eSim no network link click',
      async () => {
        eSimManagerRemote.addEuiccForTest(0);
        init();
        addESimSlot();
        await flushAsync();
        const esimNoNetworkAnchor =
            cellularNetworkList.$$('#eSimNoNetworkFound')
                .querySelector('settings-localized-link')
                .shadowRoot.querySelector('a');
        assertTrue(!!esimNoNetworkAnchor);

        const showEsimCellularSetupPromise = test_util.eventToPromise(
            'show-cellular-setup', cellularNetworkList);
        esimNoNetworkAnchor.click();
        const eSimCellularEvent = await showEsimCellularSetupPromise;
        assertEquals(
            eSimCellularEvent.detail.pageName,
            cellularSetup.CellularSetupPageName.ESIM_FLOW_UI);
      });

  test('Show EID and QR code popup', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    init();
    addESimSlot();
    await flushAsync();
    let eidPopup = cellularNetworkList.$$('.eid-popup');
    assertFalse(!!eidPopup);
    const eidPopupBtn = cellularNetworkList.$$('#eidPopupButton');
    assertTrue(!!eidPopupBtn);

    eidPopupBtn.click();
    await flushAsync();

    eidPopup = cellularNetworkList.$$('.eid-popup');
    assertTrue(!!eidPopup);
  });

  test('Install pending eSIM profile', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    init();
    addESimSlot();
    cellularNetworkList.isConnectedToNonCellularNetwork = true;
    await flushAsync();

    let eSimNetworkList = cellularNetworkList.$$('#esimNetworkList');
    assertTrue(!!eSimNetworkList);

    Polymer.dom.flush();

    const listItem = eSimNetworkList.$$('network-list-item');
    assertTrue(!!listItem);
    const installButton = listItem.$$('#installButton');
    assertTrue(!!installButton);
    installButton.click();

    await flushAsync();

    // eSIM network list should now be hidden and link showing.
    eSimNetworkList = cellularNetworkList.$$('#esimNetworkList');
    assertFalse(!!eSimNetworkList);
    const esimNoNetworkAnchor = cellularNetworkList.$$('#eSimNoNetworkFound')
                                    .querySelector('settings-localized-link')
                                    .shadowRoot.querySelector('a');
    assertTrue(!!esimNoNetworkAnchor);
  });

  test(
      'Hide esim section when no EUICC is found or no eSIM slots', async () => {
        init();
        setManagedPropertiesForTest(mojom.NetworkType.kCellular, [
          OncMojo.getDefaultManagedProperties(
              mojom.NetworkType.kTether, 'tether1'),
        ]);
        Polymer.dom.flush();
        await flushAsync();
        // The list should be hidden with no EUICC or eSIM slots.
        assertFalse(!!cellularNetworkList.$$('#esimNetworkList'));

        // Add an eSIM slot.
        await addESimSlot();
        // The list should still be hidden.
        assertFalse(!!cellularNetworkList.$$('#esimNetworkList'));

        // Add an EUICC.
        eSimManagerRemote.addEuiccForTest(1);
        await flushAsync();
        // The list should now be showing
        assertTrue(!!cellularNetworkList.$$('#esimNetworkList'));

        // Remove the eSIM slot
        clearSimSlots();
        // The list should be hidden again.
        assertFalse(!!cellularNetworkList.$$('#esimNetworkList'));
      });

  test('Hide pSIM section when no pSIM slots', async () => {
    init();
    setManagedPropertiesForTest(mojom.NetworkType.kCellular, [
      OncMojo.getDefaultManagedProperties(mojom.NetworkType.kTether, 'tether1'),
    ]);
    await flushAsync();
    assertFalse(!!cellularNetworkList.$$('#pSimNoNetworkFound'));

    addPSimSlot();
    assertTrue(!!cellularNetworkList.$$('#pSimNoNetworkFound'));

    clearSimSlots();
    assertFalse(!!cellularNetworkList.$$('#pSimNoNetworkFound'));
  });

  test('Hide instant tethering section when not enabled', async () => {
    init();
    assertFalse(!!cellularNetworkList.$$('#tetherNetworksNotSetup'));

    browserProxy.setInstantTetheringStateForTest(
        settings.MultiDeviceFeatureState.ENABLED_BY_USER);
    await flushAsync();
    assertTrue(!!cellularNetworkList.$$('#tetherNetworksNotSetup'));

    browserProxy.setInstantTetheringStateForTest(
        settings.MultiDeviceFeatureState.UNAVAILABLE_NO_VERIFIED_HOST);
    await flushAsync();
    assertFalse(!!cellularNetworkList.$$('#tetherNetworksNotSetup'));
  });

  test(
      'Fire show toast event if download profile clicked without' +
          'non-cellular connection.',
      async () => {
        eSimManagerRemote.addEuiccForTest(1);
        init();
        addESimSlot();
        cellularNetworkList.isConnectedToNonCellularNetwork = false;
        await flushAsync();

        const eSimNetworkList = cellularNetworkList.$$('#esimNetworkList');
        assertTrue(!!eSimNetworkList);

        Polymer.dom.flush();

        const listItem = eSimNetworkList.$$('network-list-item');
        assertTrue(!!listItem);
        const installButton = listItem.$$('#installButton');
        assertTrue(!!installButton);

        const showErrorToastPromise =
            test_util.eventToPromise('show-error-toast', cellularNetworkList);
        installButton.click();

        const showErrorToastEvent = await showErrorToastPromise;
        assertEquals(
            showErrorToastEvent.detail,
            cellularNetworkList.i18n('eSimNoConnectionErrorToast'));
      });

  test('Fire show cellular setup event on add cellular clicked', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    init();
    const eSimNetwork1 = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular_esim1');
    eSimNetwork1.typeProperties.cellular.eid =
        '11111111111111111111111111111111';
    setManagedPropertiesForTest(mojom.NetworkType.kCellular, [
      OncMojo.getDefaultManagedProperties(
          mojom.NetworkType.kCellular, 'cellular1'),
      eSimNetwork1,
    ]);
    cellularNetworkList.cellularDeviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: mojom.DeviceStateType.kEnabled,
      inhibitReason: mojom.InhibitReason.kNotInhibited
    };
    addESimSlot();
    cellularNetworkList.globalPolicy = {
      allowOnlyPolicyNetworksToConnect: true,
    };
    await flushAsync();

    // When policy is enabled add cellular button should not be shown.
    let addESimButton = cellularNetworkList.$$('#addESimButton');
    assertFalse(!!addESimButton);

    cellularNetworkList.globalPolicy = {
      allowOnlyPolicyNetworksToConnect: false,
    };

    await flushAsync();
    addESimButton = cellularNetworkList.$$('#addESimButton');
    assertTrue(!!addESimButton);
    assertFalse(addESimButton.disabled);

    // When device is inhibited add cellular button should be disabled.
    cellularNetworkList.cellularDeviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: mojom.DeviceStateType.kEnabled,
      inhibitReason: mojom.InhibitReason.kInstallingProfile
    };
    addESimSlot();
    await flushAsync();
    assertTrue(!!addESimButton);
    assertTrue(addESimButton.disabled);

    // Device is not inhibited and policy is also false add cellular button
    // should be enabled
    cellularNetworkList.cellularDeviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: mojom.DeviceStateType.kEnabled,
      inhibitReason: mojom.InhibitReason.kNotInhibited
    };
    addESimSlot();
    await flushAsync();
    assertTrue(!!addESimButton);
    assertFalse(addESimButton.disabled);

    const showCellularSetupPromise =
        test_util.eventToPromise('show-cellular-setup', cellularNetworkList);
    addESimButton.click();
    await Promise.all([showCellularSetupPromise, test_util.flushTasks()]);
  });

});
