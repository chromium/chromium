// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_list_item.m.js';

// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/services/network/public/mojom/ip_address.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-lite.js';

// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {NetworkList} from 'chrome://resources/cr_components/chromeos/network/network_list_types.m.js';
// #import {keyDownOn, move} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {eventToPromise} from 'chrome://test/test_util.m.js';
// #import {CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
// clang-format on

suite('NetworkListItemTest', function() {
  /** @type {!NetworkListItem|undefined} */
  let listItem;
  let mojom;
  let eSimManagerRemote = null;

  /** @type {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote|undefined} */
  let mojoApi_;

  let eventTriggered;

  setup(function() {
    loadTimeData.overrideValues({
      updatedCellularActivationUi: true,
    });

    mojom = chromeos.networkConfig.mojom;
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);
    listItem = document.createElement('network-list-item');
    listItem.showButtons = true;
    setEventListeners();
    eventTriggered = false;
    document.body.appendChild(listItem);
    Polymer.dom.flush();
  });

  function initCellularNetwork(iccid, eid, simLocked) {
    const properties = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular');
    properties.typeProperties.cellular.iccid = iccid;
    properties.typeProperties.cellular.eid = eid;
    properties.typeProperties.cellular.simLocked = simLocked;
    mojoApi_.setManagedPropertiesForTest(properties);
    return OncMojo.managedPropertiesToNetworkState(properties);
  }

  function setEventListeners() {
    listItem.addEventListener('show-detail', (event) => {
      eventTriggered = true;
    });
    listItem.addEventListener('custom-item-selected', (event) => {
      eventTriggered = true;
    });
    listItem.addEventListener('install-profile', (event) => {
      eventTriggered = true;
    });
    listItem.addEventListener('selected', (event) => {
      eventTriggered = true;
    });
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function enter() {
    keyDownOn(listItem.$$('#divOuter'), 0, [], 'Enter');
  }

  test('Network icon visibility', function() {
    // The network icon is not shown if there is no network state.
    let networkIcon = listItem.$$('network-icon');
    assertFalse(!!networkIcon);

    const properties = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kEthernet, 'eth0');
    mojoApi_.setManagedPropertiesForTest(properties);
    listItem.item = OncMojo.managedPropertiesToNetworkState(properties);

    // Update the network state.
    Polymer.dom.flush();

    // The network icon exists now.
    networkIcon = listItem.$$('network-icon');
    assertTrue(!!networkIcon);
  });

  test('Network provider name visibilty', async () => {
    const properties = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kEthernet, 'eth0');
    mojoApi_.setManagedPropertiesForTest(properties);
    listItem.item = OncMojo.managedPropertiesToNetworkState(properties);
    await flushAsync();

    let providerName = listItem.$$('#subtitle');
    assertFalse(!!providerName.textContent.trim());

    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const networkState = initCellularNetwork(/*iccid=*/ '1', /*eid=*/ '1');
    listItem.item = networkState;
    await flushAsync();

    providerName = listItem.$$('#subtitle');
    assertTrue(!!providerName);
    assertEquals('provider1', providerName.textContent.trim());
  });

  test('Pending activation pSIM UI visibility', async () => {
    const networkStateText = listItem.$.networkStateText;
    assertTrue(!!networkStateText);
    assertTrue(networkStateText.hidden);
    assertFalse(!!listItem.$$('#activateButton'));

    // Set item to an activated pSIM network first.
    const managedPropertiesActivated = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular');
    managedPropertiesActivated.typeProperties.cellular.activationState =
        mojom.ActivationStateType.kActivated;
    managedPropertiesActivated.typeProperties.cellular.paymentPortal = {
      url: 'url'
    };
    mojoApi_.setManagedPropertiesForTest(managedPropertiesActivated);

    listItem.item =
        OncMojo.managedPropertiesToNetworkState(managedPropertiesActivated);
    await flushAsync();

    // Activate button should not be showing.
    assertFalse(!!listItem.$$('#activateButton'));
    // Network state text should not be showing.
    assertTrue(networkStateText.hidden);

    // Set item to an unactivated eSIM network with a payment URL.
    const managedPropertiesESimNotActivated =
        OncMojo.getDefaultManagedProperties(
            mojom.NetworkType.kCellular, 'cellular');
    managedPropertiesESimNotActivated.typeProperties.cellular.eid = 'eid';
    managedPropertiesESimNotActivated.typeProperties.cellular.activationState =
        mojom.ActivationStateType.kNotActivated;
    managedPropertiesESimNotActivated.typeProperties.cellular.paymentPortal = {
      url: 'url'
    };
    mojoApi_.setManagedPropertiesForTest(managedPropertiesESimNotActivated);

    listItem.item = OncMojo.managedPropertiesToNetworkState(
        managedPropertiesESimNotActivated);
    await flushAsync();

    // Activate button should not be showing.
    assertFalse(!!listItem.$$('#activateButton'));

    // Set item to an unactivated pSIM network with a payment URL.
    const managedPropertiesNotActivated = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular');
    managedPropertiesNotActivated.typeProperties.cellular.activationState =
        mojom.ActivationStateType.kNotActivated;
    managedPropertiesNotActivated.typeProperties.cellular.paymentPortal = {
      url: 'url'
    };
    mojoApi_.setManagedPropertiesForTest(managedPropertiesNotActivated);

    const networkState =
        OncMojo.managedPropertiesToNetworkState(managedPropertiesNotActivated);
    listItem.item = networkState;

    await flushAsync();

    // Activate button should now be showing.
    const activateButton = listItem.$$('#activateButton');
    assertTrue(!!activateButton);
    // Network state text should not be showing.
    assertTrue(networkStateText.hidden);

    // Arrow button should also be visible.
    const arrow = listItem.$$('#subpageButton');
    assertTrue(!!arrow);

    // Clicking the activate button should fire the show-cellular-setup event.
    const showCellularSetupPromise =
        test_util.eventToPromise('show-cellular-setup', listItem);
    activateButton.click();
    const showCellularSetupEvent = await showCellularSetupPromise;
    assertEquals(
        showCellularSetupEvent.detail.pageName,
        cellularSetup.CellularSetupPageName.PSIM_FLOW_UI);

    // Selecting the row should fire the show-detail event.
    const showDetailPromise = test_util.eventToPromise('show-detail', listItem);
    listItem.$.divOuter.click();
    const showDetailEvent = await showDetailPromise;
    assertEquals(showDetailEvent.detail, networkState);
  });

  test('Unavailable pSIM UI visibility', async () => {
    const networkStateText = listItem.$.networkStateText;
    assertTrue(!!networkStateText);
    assertTrue(networkStateText.hidden);
    assertFalse(!!listItem.$$('#activateButton'));

    // Set item to an unactivated eSIM network without a payment URL.
    const managedPropertiesESimUnavailable =
        OncMojo.getDefaultManagedProperties(
            mojom.NetworkType.kCellular, 'cellular');
    managedPropertiesESimUnavailable.typeProperties.cellular.eid = 'eid';
    managedPropertiesESimUnavailable.typeProperties.cellular.activationState =
        mojom.ActivationStateType.kNotActivated;
    managedPropertiesESimUnavailable.typeProperties.cellular.paymentPortal = {};
    mojoApi_.setManagedPropertiesForTest(managedPropertiesESimUnavailable);

    listItem.item = OncMojo.managedPropertiesToNetworkState(
        managedPropertiesESimUnavailable);
    await flushAsync();

    // Activate button should not be showing.
    assertFalse(!!listItem.$$('#activateButton'));
    // Network state text should not be showing.
    assertTrue(networkStateText.hidden);

    // Set item to an unactivated pSIM network without a payment URL.
    const managedPropertiesUnavailable = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular');
    managedPropertiesUnavailable.typeProperties.cellular.activationState =
        mojom.ActivationStateType.kNotActivated;
    managedPropertiesUnavailable.typeProperties.cellular.paymentPortal = {};
    mojoApi_.setManagedPropertiesForTest(managedPropertiesUnavailable);

    const networkState =
        OncMojo.managedPropertiesToNetworkState(managedPropertiesUnavailable);
    listItem.item = networkState;
    await flushAsync();

    // Activate button should not be showing.
    assertFalse(!!listItem.$$('#activateButton'));
    // Network state text should be showing.
    assertFalse(networkStateText.hidden);
    assertTrue(networkStateText.classList.contains('warning'));
    assertEquals(
        networkStateText.textContent.trim(),
        listItem.i18n('networkListItemUnavailableSimNetwork'));

    // Arrow button should still be visible.
    const arrow = listItem.$$('#subpageButton');
    assertTrue(!!arrow);

    // Selecting the row should fire the show-detail event.
    const showDetailPromise = test_util.eventToPromise('show-detail', listItem);
    listItem.$.divOuter.click();
    const showDetailEvent = await showDetailPromise;
    assertEquals(showDetailEvent.detail, networkState);
  });

  test('Activating pSIM spinner visibility', async () => {
    assertFalse(!!listItem.$$('#activatingPSimSpinner'));

    // Set item to an activated pSIM network first.
    const managedPropertiesActivated = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular');
    managedPropertiesActivated.typeProperties.cellular.activationState =
        mojom.ActivationStateType.kActivated;
    mojoApi_.setManagedPropertiesForTest(managedPropertiesActivated);

    listItem.item =
        OncMojo.managedPropertiesToNetworkState(managedPropertiesActivated);
    await flushAsync();

    // Activating spinner should not be showing.
    assertFalse(!!listItem.$$('#activatingPSimSpinner'));

    // Set item to an activating eSIM network.
    const managedPropertiesESimActivating = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular');

    managedPropertiesESimActivating.typeProperties.cellular.eid = 'eid';
    managedPropertiesESimActivating.typeProperties.cellular.activationState =
        mojom.ActivationStateType.kActivating;
    mojoApi_.setManagedPropertiesForTest(managedPropertiesESimActivating);

    listItem.item = OncMojo.managedPropertiesToNetworkState(
        managedPropertiesESimActivating);
    await flushAsync();

    // Activating spinner should not be showing.
    assertFalse(!!listItem.$$('#activatingPSimSpinner'));

    // Set item to an activating pSIM network.
    const managedPropertiesActivating = OncMojo.getDefaultManagedProperties(
        mojom.NetworkType.kCellular, 'cellular');
    managedPropertiesActivating.typeProperties.cellular.activationState =
        mojom.ActivationStateType.kActivating;
    mojoApi_.setManagedPropertiesForTest(managedPropertiesActivating);

    const networkState =
        OncMojo.managedPropertiesToNetworkState(managedPropertiesActivating);
    listItem.item = networkState;
    await flushAsync();

    // Activating spinner should now be showing.
    assertTrue(!!listItem.$$('#activatingPSimSpinner'));

    // Arrow button should also be visible.
    let arrow = listItem.$$('#subpageButton');
    assertTrue(!!arrow);

    // Selecting the row should fire the show-detail event.
    const showDetailPromise = test_util.eventToPromise('show-detail', listItem);
    listItem.$.divOuter.click();
    const showDetailEvent = await showDetailPromise;
    assertEquals(showDetailEvent.detail, networkState);
  });

  test(
      'Pending eSIM profile name, provider, install button visibilty',
      async () => {
        const itemName = 'Item Name';
        const itemSubtitle = 'Item Subtitle';
        listItem.item = {
          customItemType: NetworkList.CustomItemType.ESIM_PENDING_PROFILE,
          customItemName: itemName,
          customItemSubtitle: itemSubtitle,
          polymerIcon: 'network:cellular-0',
          showBeforeNetworksList: false,
          customData: {
            iccid: 'iccid',
          },
        };
        await flushAsync();

        let networkName = listItem.$$('#networkName');
        assertTrue(!!networkName);
        assertEquals(itemName, networkName.textContent.trim());

        let subtitle = listItem.$$('#subtitle');
        assertTrue(!!subtitle);
        assertEquals(itemSubtitle, subtitle.textContent.trim());

        let installButton = listItem.$$('#installButton');
        assertTrue(!!installButton);

        let installProfileEventIccid = null;
        listItem.addEventListener('install-profile', (event) => {
          installProfileEventIccid = event.detail.iccid;
        });
        installButton.click();

        await flushAsync();
        assertEquals(installProfileEventIccid, 'iccid');
      });

  test(
      'Installing eSIM profile name, provider, spinner visibilty', async () => {
        const itemName = 'Item Name';
        const itemSubtitle = 'Item Subtitle';
        listItem.item = {
          customItemType: NetworkList.CustomItemType.ESIM_INSTALLING_PROFILE,
          customItemName: itemName,
          customItemSubtitle: itemSubtitle,
          polymerIcon: 'network:cellular-0',
          showBeforeNetworksList: false,
          customData: {
            iccid: 'iccid',
          },
        };
        await flushAsync();

        let networkName = listItem.$$('#networkName');
        assertTrue(!!networkName);
        assertEquals(itemName, networkName.textContent.trim());

        let subtitle = listItem.$$('#subtitle');
        assertTrue(!!subtitle);
        assertEquals(itemSubtitle, subtitle.textContent.trim());

        let spinner = listItem.$$('#installingESimSpinner');
        assertTrue(!!spinner);
      });

  test('Only active SIMs should show scanning subtext', async () => {
    const kTestIccid1 = '00000000000000000000';
    const kTestIccid2 = '11111111111111111111';
    const kTestEid = '1';
    const networkStateText = listItem.$$('#networkStateText');

    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const cellularNetwork1 = initCellularNetwork(kTestIccid1, kTestEid);
    const cellularNetwork2 = initCellularNetwork(kTestIccid2, /*eid=*/ '');

    // Assert that state text is hidden for inactive SIM.
    listItem.deviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: mojom.DeviceStateType.kEnabled,
      simInfos: [
        {slot_id: 1, eid: kTestEid, iccid: kTestIccid1, isPrimary: false},
        {slot_id: 2, eid: '', iccid: kTestIccid2, isPrimary: true}
      ],
      scanning: true
    };
    listItem.item = cellularNetwork1;
    await flushAsync();
    assertTrue(networkStateText.hidden);

    // Assert that scanning subtext is shown for active SIM.
    listItem.item = cellularNetwork2;
    await flushAsync();
    assertFalse(networkStateText.hidden);
    assertEquals(
        networkStateText.textContent.trim(),
        listItem.i18n('networkListItemScanning'));
  });

  test('Show sim lock dialog when cellular network is locked', async () => {
    const iccid = '11111111111111111111';
    const eid = '1';
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const networkStateLockedText =
        listItem.i18n('networkListItemUpdatedCellularSimCardLocked');

    listItem.item = initCellularNetwork(iccid, eid, /*simlocked=*/ false);

    await flushAsync();

    let unlockBtn = listItem.$$('#unlockButton');

    // Arrow button should be visible when unlock button is not visible.
    let arrow = listItem.$$('#subpageButton');
    assertTrue(!!arrow);

    assertFalse(!!unlockBtn);
    let networkStateText = listItem.$$('#networkStateText');
    assertTrue(!!networkStateText);
    assertNotEquals(
        networkStateLockedText, networkStateText.textContent.trim());

    listItem.set('networkState.typeState.cellular.simLocked', true);
    await flushAsync();
    // Arrow button should be hidden when unlock button is visible.
    arrow = listItem.$$('#subpageButton');
    assertFalse(!!arrow);

    await flushAsync();
    unlockBtn = listItem.$$('#unlockButton');
    let simLockDialog = listItem.$$('sim-lock-dialogs');
    assertTrue(!!unlockBtn);
    assertFalse(!!simLockDialog);

    unlockBtn.click();
    await flushAsync();

    simLockDialog = listItem.$$('sim-lock-dialogs');
    assertTrue(!!simLockDialog);
    networkStateText = listItem.$$('#networkStateText');
    assertTrue(!!networkStateText);
    assertEquals(networkStateLockedText, networkStateText.textContent.trim());
  });

  test('Disable sim lock button when device is inhibited', async () => {
    const iccid = '11111111111111111111';
    const eid = '1';
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);

    listItem.item = initCellularNetwork(iccid, eid, /*simlocked=*/ true);
    listItem.deviceState = {
      type: mojom.NetworkType.kCellular,
      inhibitedReason: mojom.InhibitReason.kInstallingProfile,
    };

    await flushAsync();

    let unlockBtn = listItem.$$('#unlockButton');
    assertTrue(!!unlockBtn.disabled);
  });

  test('Network disabled, Pending eSIM, install button visible', async () => {
    const itemName = 'Item Name';
    const itemSubtitle = 'Item Subtitle';
    listItem.item = {
      customItemType: NetworkList.CustomItemType.ESIM_PENDING_PROFILE,
      customItemName: itemName,
      customItemSubtitle: itemSubtitle,
      polymerIcon: 'network:cellular-0',
      showBeforeNetworksList: false,
      customData: {
        iccid: 'iccid',
      },
    };
    listItem.deviceState = {
      type: mojom.NetworkType.kCellular,
      inhibitedReason: mojom.InhibitReason.kInstallingProfile,
    };

    await flushAsync();

    listItem.deviceState.inhibitedReason =
        mojom.InhibitReason.kInstallingProfile;

    let installButton = listItem.$$('#installButton');
    assertTrue(!!installButton);
    assertTrue(installButton.disabled);

    installButton.click();
    await flushAsync();
    assertFalse(eventTriggered);

    listItem.$$('#divOuter').click();
    await flushAsync();
    assertFalse(eventTriggered);

    enter();
    await flushAsync();
    assertFalse(eventTriggered);
  });

  test(
      'Network disabled, no arrow and enter and click does not fire events',
      async () => {
        const properties = OncMojo.getDefaultManagedProperties(
            mojom.NetworkType.kCellular, 'cellular');
        mojoApi_.setManagedPropertiesForTest(properties);
        listItem.networkState =
            OncMojo.managedPropertiesToNetworkState(properties);
        listItem.deviceState = {
          type: mojom.NetworkType.kCellular,
          inhibitedReason: mojom.InhibitReason.kInstallingProfile,
        };
        await flushAsync();

        let arrow = listItem.$$('#subpageButton');
        assertFalse(!!arrow);

        listItem.$$('#divOuter').click();
        await flushAsync();
        assertFalse(eventTriggered);

        enter();
        await flushAsync();
        assertFalse(eventTriggered);
      });

  test('Show locked sublabel when cellular network is locked', async () => {
    const iccid = '11111111111111111111';
    const eid = '1';
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const networkStateLockedText =
        listItem.i18n('networkListItemUpdatedCellularSimCardLocked');

    listItem.item = initCellularNetwork(iccid, eid, /*simlocked=*/ true);

    await flushAsync();
    const networkStateText = listItem.$$('#networkStateText');
    assertTrue(!!networkStateText);
    assertEquals(networkStateLockedText, networkStateText.textContent.trim());
  });

  test(
      'Show locked sublabel when cellular network is locked and scanning',
      async () => {
        const iccid = '11111111111111111111';
        const eid = '1';
        eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
        const networkStateLockedText =
            listItem.i18n('networkListItemUpdatedCellularSimCardLocked');
        const networkStateScanningText =
            listItem.i18n('networkListItemScanning');

        listItem.item = initCellularNetwork(iccid, eid, /*simlocked=*/ true);
        listItem.deviceState = {scanning: true};

        await flushAsync();
        const networkStateText = listItem.$$('#networkStateText');
        assertTrue(!!networkStateText);
        assertEquals(
            networkStateLockedText, networkStateText.textContent.trim());
        assertNotEquals(
            networkStateScanningText, networkStateText.textContent.trim());
      });

  test('Cellular network item standard height', async () => {
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const networkState = initCellularNetwork(/*iccid=*/ '1', /*eid=*/ '1');
    networkState.connectionState = mojom.ConnectionStateType.kNotConnected;
    listItem.item = networkState;

    const networkStateText = listItem.$$('#networkStateText');
    networkStateText.hidden = true;
    networkStateText.active = false;
    await flushAsync();
    assertTrue(networkStateText.hidden);
    assertFalse(networkStateText.active);
    assertEquals(networkStateText.textContent.trim(), '');

    const networkName = listItem.$$('#networkName');
    assertFalse(networkName.hidden);

    const subtitle = listItem.$$('#subtitle');
    subtitle.hidden = false;
    await flushAsync();
    assertFalse(subtitle.hidden);

    const divOuter = listItem.$$('#divOuter');
    assertTrue(!!divOuter);
    assertTrue(divOuter.classList.contains('div-outer-with-standard-height'));
    assertFalse(divOuter.classList.contains('div-outer-with-subtitle-height'));
  });

  test('Cellular network item subtitle height', async () => {
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const networkState = initCellularNetwork(/*iccid=*/ '1', /*eid=*/ '1');
    networkState.connectionState = mojom.ConnectionStateType.kConnected;
    listItem.item = networkState;

    const networkStateText = listItem.$$('#networkStateText');
    networkStateText.hidden = false;
    networkStateText.active = true;
    await flushAsync();
    assertTrue(!!networkStateText);
    assertFalse(networkStateText.hidden);
    assertTrue(networkStateText.active);
    assertEquals(
        networkStateText.textContent.trim(),
        listItem.i18n('networkListItemConnected'));

    const networkName = listItem.$$('#networkName');
    assertFalse(!!networkName.hidden);

    const subtitle = listItem.$$('#subtitle');
    subtitle.hidden = false;
    await flushAsync();
    assertTrue(!!subtitle);
    assertFalse(subtitle.hidden);

    const divOuter = listItem.$$('#divOuter');
    assertTrue(!!divOuter);
    assertFalse(divOuter.classList.contains('div-outer-with-standard-height'));
    assertTrue(divOuter.classList.contains('div-outer-with-subtitle-height'));
  });

});
