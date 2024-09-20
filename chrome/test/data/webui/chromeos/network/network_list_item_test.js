// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_list_item.js';

import {CellularSetupPageName} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkList} from 'chrome://resources/ash/common/network/network_list_types.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ActivationStateType, CrosNetworkConfigRemote, InhibitReason, SecurityType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType, OncSource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('NetworkListItemTest', function() {
  /** @type {!NetworkListItem|undefined} */
  let listItem;
  let eSimManagerRemote = null;

  /** @type {!CrosNetworkConfigRemote|undefined} */
  let mojoApi_;

  let eventTriggered;

  setup(function() {
    loadTimeData.overrideValues({
      'isUserLoggedIn': true,
    });
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);
  });

  function init() {
    listItem = document.createElement('network-list-item');
    listItem.showButtons = true;
    setEventListeners();
    eventTriggered = false;
    document.body.appendChild(listItem);
    flush();
  }

  function initCellularNetwork(iccid, eid, simLocked, simLockType, name) {
    const properties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular', name);
    properties.typeProperties.cellular.iccid = iccid;
    properties.typeProperties.cellular.eid = eid;
    properties.typeProperties.cellular.simLocked = simLocked;
    mojoApi_.setManagedPropertiesForTest(properties);
    const networkState = OncMojo.managedPropertiesToNetworkState(properties);
    networkState.typeState.cellular.simLockType = simLockType;
    return networkState;
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
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function enter() {
    keyDownOn(listItem.$$('#divOuter'), 0, [], 'Enter');
  }

  test('Network icon visibility', function() {
    init();

    // The network icon is not shown if there is no network state.
    let networkIcon = listItem.$$('network-icon');
    assertFalse(!!networkIcon);

    const properties =
        OncMojo.getDefaultManagedProperties(NetworkType.kEthernet, 'eth0');
    mojoApi_.setManagedPropertiesForTest(properties);
    listItem.item = OncMojo.managedPropertiesToNetworkState(properties);

    // Update the network state.
    flush();

    // The network icon exists now.
    networkIcon = listItem.$$('network-icon');
    assertTrue(!!networkIcon);
  });

  test('Network provider name visibilty', async () => {
    init();

    const getTitle = () => {
      const element = listItem.$$('#itemTitle');
      return element ? element.textContent.trim() : '';
    };

    const euicc = eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 2);
    const profiles = (await euicc.getProfileList()).profiles;
    assertEquals(2, profiles.length);
    profiles[0].setDeferGetProperties(/*defer=*/ true);
    profiles[1].setDeferGetProperties(/*defer=*/ true);

    const networkState1 = initCellularNetwork(/*iccid=*/ '1', /*eid=*/ '1');
    const networkState2 = initCellularNetwork(/*iccid=*/ '2', /*eid=*/ '1');

    // Change network states to simulate list item recycling.
    listItem.item = networkState1;
    await flushAsync();
    listItem.item = networkState2;
    await flushAsync();

    // Allow last getESimProfileProperties call for networkState2 to complete.
    profiles[0].resolveLastGetPropertiesPromise();
    await flushAsync();
    profiles[1].resolveLastGetPropertiesPromise();
    await flushAsync();

    // Simulate getESimProfileProperties for networkState1 resolving out of
    // order.
    profiles[0].resolveLastGetPropertiesPromise();
    await flushAsync();

    // Verify that full title is displayed correctly even if promise from
    // previous network resolves out of order.
    assertEquals(
        listItem.i18n('networkListItemTitle', 'Cellular', 'provider2'),
        getTitle());

    // Change to network state without provider name and verify that that title
    // is displayed correctly.
    const ethernetProperties =
        OncMojo.getDefaultManagedProperties(NetworkType.kEthernet, 'eth0');
    mojoApi_.setManagedPropertiesForTest(ethernetProperties);
    listItem.item = OncMojo.managedPropertiesToNetworkState(ethernetProperties);
    await flushAsync();
    assertEquals('Ethernet', getTitle());
  });

  test('eSIM network title', async () => {
    init();

    const getTitle = () => {
      const element = listItem.$$('#itemTitle');
      return element ? element.textContent.trim() : '';
    };

    const euicc = eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const providerName = 'provider1';
    listItem.item = initCellularNetwork(
        /*iccid=*/ '1', /*eid=*/ '1', /*simlock=*/ false, /*simlocktype*/ '',
        'nickname');
    await flushAsync();
    assertEquals(
        listItem.i18n('networkListItemTitle', 'nickname', providerName),
        getTitle());

    // Change eSIM network's name to the same as provider name, verifies that
    // the title only show the network name.
    listItem.item = initCellularNetwork(
        /*iccid=*/ '1', /*eid=*/ '1', /*simlock=*/ false, /*simlocktype*/ '',
        providerName);
    await flushAsync();
    assertEquals(providerName, getTitle());
  });

  test('Network title does not allow XSS', async () => {
    init();

    const getTitle = () => {
      const element = listItem.$$('#itemTitle');
      return element ? element.textContent.trim() : '';
    };

    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);

    const badName = '<script>alert("Bad Name");</script>';
    listItem.item = initCellularNetwork(
        /*iccid=*/ '1', /*eid=*/ '1', /*simlock=*/ false,
        /*simlocktype*/ '', /*name=*/ badName);
    await flushAsync();
    assertTrue(!!listItem);
    assertTrue(getTitle().startsWith(badName));
  });

  test('Pending activation pSIM UI visibility', async () => {
    init();

    const sublabel = listItem.$.sublabel;
    assertTrue(!!sublabel);
    assertTrue(sublabel.hidden);
    assertFalse(!!listItem.$$('#activateButton'));

    // Set item to an activated pSIM network first.
    const managedPropertiesActivated =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular');
    managedPropertiesActivated.typeProperties.cellular.iccid = '100000';
    managedPropertiesActivated.typeProperties.cellular.activationState =
        ActivationStateType.kActivated;
    managedPropertiesActivated.typeProperties.cellular.paymentPortal = {
      url: 'url',
    };
    mojoApi_.setManagedPropertiesForTest(managedPropertiesActivated);

    listItem.item =
        OncMojo.managedPropertiesToNetworkState(managedPropertiesActivated);
    await flushAsync();

    // Activate button should not be showing.
    assertFalse(!!listItem.$$('#activateButton'));
    // Network state text should not be showing.
    assertTrue(sublabel.hidden);

    // Set item to an unactivated eSIM network with a payment URL.
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const managedPropertiesESimNotActivated =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular');
    managedPropertiesESimNotActivated.typeProperties.cellular.eid = 'eid';
    managedPropertiesESimNotActivated.typeProperties.cellular.iccid = '100000';
    managedPropertiesESimNotActivated.typeProperties.cellular.activationState =
        ActivationStateType.kNotActivated;
    managedPropertiesESimNotActivated.typeProperties.cellular.paymentPortal = {
      url: 'url',
    };
    mojoApi_.setManagedPropertiesForTest(managedPropertiesESimNotActivated);

    listItem.item = OncMojo.managedPropertiesToNetworkState(
        managedPropertiesESimNotActivated);
    await flushAsync();

    // Activate button should not be showing.
    assertFalse(!!listItem.$$('#activateButton'));
    assertFalse(sublabel.hidden);

    // Set item to an unactivated pSIM network with a payment URL.
    const managedPropertiesNotActivated =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular');
    managedPropertiesNotActivated.typeProperties.cellular.iccid = '100000';
    managedPropertiesNotActivated.typeProperties.cellular.activationState =
        ActivationStateType.kNotActivated;
    managedPropertiesNotActivated.typeProperties.cellular.paymentPortal = {
      url: 'url',
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
    assertTrue(sublabel.hidden);

    // Arrow button should also be visible.
    const arrow = listItem.$$('#subpageButton');
    assertTrue(!!arrow);

    // Clicking the activate button should fire the show-cellular-setup event.
    const showCellularSetupPromise =
        eventToPromise('show-cellular-setup', listItem);
    activateButton.click();
    const showCellularSetupEvent = await showCellularSetupPromise;
    assertEquals(
        showCellularSetupEvent.detail.pageName,
        CellularSetupPageName.PSIM_FLOW_UI);

    // Selecting the row should fire the show-detail event.
    const showDetailPromise = eventToPromise('show-detail', listItem);
    listItem.$.divOuter.click();
    const showDetailEvent = await showDetailPromise;
    assertEquals(showDetailEvent.detail, networkState);

    // Setting showButtons to false should hide activate and arrow button.
    listItem.showButtons = false;
    await flushAsync();
    assertFalse(!!listItem.$$('#activateButton'));
    assertFalse(!!listItem.$$('#subpageButton'));
  });

  test('Unavailable cellular network UI visibility', async () => {
    init();

    const sublabel = listItem.$.sublabel;
    assertTrue(!!sublabel);
    assertTrue(sublabel.hidden);
    assertFalse(!!listItem.$$('#activateButton'));

    // Set item to an unactivated eSIM network without a payment URL.
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const managedPropertiesESimUnavailable =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular');
    managedPropertiesESimUnavailable.typeProperties.cellular.eid = 'eid';
    managedPropertiesESimUnavailable.typeProperties.cellular.iccid = '100000';
    managedPropertiesESimUnavailable.typeProperties.cellular.activationState =
        ActivationStateType.kNotActivated;
    managedPropertiesESimUnavailable.typeProperties.cellular.paymentPortal = {};
    managedPropertiesESimUnavailable.connectionState =
        ConnectionStateType.kConnected;
    mojoApi_.setManagedPropertiesForTest(managedPropertiesESimUnavailable);

    listItem.item = OncMojo.managedPropertiesToNetworkState(
        managedPropertiesESimUnavailable);
    await flushAsync();

    // Activate button should not be showing.
    assertFalse(!!listItem.$$('#activateButton'));
    // Network state text should be showing.
    assertFalse(sublabel.hidden);
    assertTrue(sublabel.classList.contains('warning'));
    assertEquals(
        sublabel.textContent.trim(),
        listItem.i18n('networkListItemUnavailableSimNetwork'));

    // Selecting the row should fire the show-detail event.
    let showDetailPromise = eventToPromise('show-detail', listItem);
    listItem.$.divOuter.click();
    let showDetailEvent = await showDetailPromise;
    assertEquals(showDetailEvent.detail, listItem.item);

    // Set item to an unactivated pSIM network without a payment URL.
    const managedPropertiesUnavailable =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular');
    managedPropertiesUnavailable.typeProperties.cellular.iccid = '1000000';
    managedPropertiesUnavailable.typeProperties.cellular.activationState =
        ActivationStateType.kNotActivated;
    managedPropertiesUnavailable.typeProperties.cellular.paymentPortal = {};
    mojoApi_.setManagedPropertiesForTest(managedPropertiesUnavailable);

    const networkState =
        OncMojo.managedPropertiesToNetworkState(managedPropertiesUnavailable);
    listItem.item = networkState;
    await flushAsync();

    // Activate button should not be showing.
    assertFalse(!!listItem.$$('#activateButton'));
    // Network state text should be showing.
    assertFalse(sublabel.hidden);
    assertTrue(sublabel.classList.contains('warning'));
    assertEquals(
        sublabel.textContent.trim(),
        listItem.i18n('networkListItemUnavailableSimNetwork'));

    // Arrow button should still be visible.
    const arrow = listItem.$$('#subpageButton');
    assertTrue(!!arrow);

    // Selecting the row should fire the show-detail event.
    showDetailPromise = eventToPromise('show-detail', listItem);
    listItem.$.divOuter.click();
    showDetailEvent = await showDetailPromise;
    assertEquals(showDetailEvent.detail, networkState);
  });

  test('Activating pSIM spinner visibility', async () => {
    assertFalse(!!listItem.$$('#activatingPSimSpinner'));

    // Set item to an activated pSIM network first.
    const managedPropertiesActivated =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular');
    managedPropertiesActivated.typeProperties.cellular.iccid = '100000';
    managedPropertiesActivated.typeProperties.cellular.activationState =
        ActivationStateType.kActivated;
    mojoApi_.setManagedPropertiesForTest(managedPropertiesActivated);

    listItem.item =
        OncMojo.managedPropertiesToNetworkState(managedPropertiesActivated);
    await flushAsync();

    // Activating spinner should not be showing.
    assertFalse(!!listItem.$$('#activatingPSimSpinner'));

    // Set item to an activating eSIM network.
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const managedPropertiesESimActivating =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular');

    managedPropertiesESimActivating.typeProperties.cellular.eid = 'eid';
    managedPropertiesESimActivating.typeProperties.cellular.iccid = '100000';
    managedPropertiesESimActivating.typeProperties.cellular.activationState =
        ActivationStateType.kActivating;
    mojoApi_.setManagedPropertiesForTest(managedPropertiesESimActivating);

    listItem.item = OncMojo.managedPropertiesToNetworkState(
        managedPropertiesESimActivating);
    await flushAsync();

    // Activating spinner should not be showing.
    assertFalse(!!listItem.$$('#activatingPSimSpinner'));

    // Set item to an activating pSIM network.
    const managedPropertiesActivating =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular');
    managedPropertiesActivating.typeProperties.cellular.iccid = '100000';
    managedPropertiesActivating.typeProperties.cellular.activationState =
        ActivationStateType.kActivating;
    mojoApi_.setManagedPropertiesForTest(managedPropertiesActivating);

    const networkState =
        OncMojo.managedPropertiesToNetworkState(managedPropertiesActivating);
    listItem.item = networkState;
    await flushAsync();

    // Activating spinner should now be showing.
    assertTrue(!!listItem.$$('#activatingPSimSpinner'));

    // Arrow button should also be visible.
    const arrow = listItem.$$('#subpageButton');
    assertTrue(!!arrow);

    // Selecting the row should fire the show-detail event.
    const showDetailPromise = eventToPromise('show-detail', listItem);
    listItem.$.divOuter.click();
    const showDetailEvent = await showDetailPromise;
    assertEquals(showDetailEvent.detail, networkState);
  });

  test(
      'Pending eSIM profile name, provider, install button visibilty',
      async () => {
        init();

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

        const title = listItem.$$('#itemTitle');
        assertTrue(!!title);
        assertEquals(
            listItem.i18n('networkListItemTitle', itemName, itemSubtitle),
            title.textContent.trim());

        const installButton = listItem.$$('#installButton');
        assertTrue(!!installButton);

        let installProfileEventIccid = null;
        listItem.addEventListener('install-profile', (event) => {
          installProfileEventIccid = event.detail.iccid;
        });
        installButton.click();

        await flushAsync();
        assertEquals(installProfileEventIccid, 'iccid');

        // Setting showButtons to false should hide install button.
        listItem.showButtons = false;
        await flushAsync();
        assertFalse(!!listItem.$$('#installButton'));
      });

  test(
      'Installing eSIM profile name, provider, spinner visibilty', async () => {
        init();

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

        const title = listItem.$$('#itemTitle');
        assertTrue(!!title);
        assertEquals(
            listItem.i18n('networkListItemTitle', itemName, itemSubtitle),
            title.textContent.trim());

        const spinner = listItem.$$('#installingESimSpinner');
        assertTrue(!!spinner);
      });

  test('Show sim lock dialog when cellular network is locked', async () => {
    init();

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
    let sublabel = listItem.$$('#sublabel');
    assertTrue(!!sublabel);
    assertNotEquals(networkStateLockedText, sublabel.textContent.trim());

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
    sublabel = listItem.$$('#sublabel');
    assertTrue(!!sublabel);
    assertEquals(networkStateLockedText, sublabel.textContent.trim());

    // Setting showButtons to false should hide unlock button.
    listItem.showButtons = false;
    await flushAsync();
    assertFalse(!!listItem.$$('#unlockButton'));
  });

  test('Disable sim lock button when device is inhibited', async () => {
    init();

    const iccid = '11111111111111111111';
    const eid = '1';
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);

    listItem.item = initCellularNetwork(iccid, eid, /*simlocked=*/ true);
    listItem.deviceState = {
      type: NetworkType.kCellular,
      inhibitedReason: InhibitReason.kInstallingProfile,
    };

    await flushAsync();

    const unlockBtn = listItem.$$('#unlockButton');
    assertTrue(!!unlockBtn.disabled);
  });

  test('Network disabled, Pending eSIM, install button visible', async () => {
    init();

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
      type: NetworkType.kCellular,
      inhibitedReason: InhibitReason.kInstallingProfile,
    };

    await flushAsync();

    listItem.deviceState.inhibitedReason = InhibitReason.kInstallingProfile;

    const installButton = listItem.$$('#installButton');
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

  test('Disable VPN subpage button when Always-On VPN forced on', async () => {
    init();

    const properties =
        OncMojo.getDefaultManagedProperties(NetworkType.kVPN, 'vpn');
    mojoApi_.setManagedPropertiesForTest(properties);
    listItem.networkState = OncMojo.managedPropertiesToNetworkState(properties);
    listItem.isBuiltInVpnManagementBlocked = false;
    await flushAsync();

    const arrow = listItem.$$('#subpageButton');
    let policyIcon = listItem.$$('#policyIcon');

    assertFalse(arrow.disabled, 'subpage button is falsely disabled');
    assertFalse(!!policyIcon, 'policy icon is falsely showing');

    listItem.isBuiltInVpnManagementBlocked = true;
    await flushAsync();

    policyIcon = listItem.$$('#policyIcon');
    assertTrue(arrow.disabled, 'subpage button is falsely enabled');
    assertTrue(!!policyIcon, 'policy icon is falsely hidden');
  });

  test(
      'Network disabled, no arrow and enter and click does not fire events',
      async () => {
        init();

        const properties = OncMojo.getDefaultManagedProperties(
            NetworkType.kCellular, 'cellular');
        mojoApi_.setManagedPropertiesForTest(properties);
        listItem.networkState =
            OncMojo.managedPropertiesToNetworkState(properties);
        listItem.deviceState = {
          type: NetworkType.kCellular,
          inhibitedReason: InhibitReason.kInstallingProfile,
        };
        await flushAsync();

        const arrow = listItem.$$('#subpageButton');
        assertFalse(!!arrow);

        listItem.$$('#divOuter').click();
        await flushAsync();
        assertFalse(eventTriggered);

        enter();
        await flushAsync();
        assertFalse(eventTriggered);
      });

  test(
      'Network item force disabled, no arrow and enter and click does not ' +
          'fire events',
      async () => {
        init();

        listItem.disableItem = true;
        await flushAsync();

        const arrow = listItem.$$('#subpageButton');
        assertFalse(!!arrow);

        listItem.$$('#divOuter').click();
        await flushAsync();
        assertFalse(eventTriggered);

        enter();
        await flushAsync();
        assertFalse(eventTriggered);
      });

  test('Show locked sublabel when cellular network is locked', async () => {
    init();

    const iccid = '11111111111111111111';
    const eid = '1';
    eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
    const networkStateLockedText =
        listItem.i18n('networkListItemUpdatedCellularSimCardLocked');

    listItem.item = initCellularNetwork(iccid, eid, /*simlocked=*/ true);

    await flushAsync();
    const sublabel = listItem.$$('#sublabel');
    assertTrue(!!sublabel);
    assertEquals(networkStateLockedText, sublabel.textContent.trim());
  });

  test(
      'Show carrier locked sublabel when cellular network is carrier locked',
      async () => {
        loadTimeData.overrideValues({
          'isUserLoggedIn': true,
        });
        init();
        const iccid = '11111111111111111111';
        const eid = '1';
        eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
        const networkStateLockedText =
            listItem.i18n('networkListItemUpdatedCellularSimCardCarrierLocked');
        listItem.item = initCellularNetwork(
            iccid, eid, /*simlocked=*/ true, /*simlocktype*/ 'network-pin');

        await flushAsync();
        const sublabel = listItem.$$('#sublabel');
        assertTrue(!!sublabel);
        assertEquals(networkStateLockedText, sublabel.textContent.trim());
      });

  test(
      'Show locked sublabel when cellular network is locked and scanning',
      async () => {
        init();

        const iccid = '11111111111111111111';
        const eid = '1';
        eSimManagerRemote.addEuiccForTest(/*numProfiles=*/ 1);
        const networkStateLockedText =
            listItem.i18n('networkListItemUpdatedCellularSimCardLocked');

        listItem.item = initCellularNetwork(iccid, eid, /*simlocked=*/ true);
        listItem.deviceState = {scanning: true};

        await flushAsync();
        const sublabel = listItem.$$('#sublabel');
        assertTrue(!!sublabel);
        assertEquals(networkStateLockedText, sublabel.textContent.trim());
      });

  test('computeIsBlockedNetwork()_ should return expected value', async () => {
    init();
    await flushAsync();
    // Should return false if item is null or undefined.
    assertFalse(listItem.computeIsBlockedNetwork_());

    // Set item to a policy blocked wifi network.
    const managedProperties =
        OncMojo.getDefaultManagedProperties(NetworkType.kWiFi, 'wifiguid');
    managedProperties.source = OncSource.kUser;
    managedProperties.typeProperties.wifi.security = SecurityType.kWepPsk;
    mojoApi_.setManagedPropertiesForTest(managedProperties);
    const networkState =
        OncMojo.managedPropertiesToNetworkState(managedProperties);
    listItem.item = networkState;
    // Set global policy to restrict managed wifi networks.
    listItem.globalPolicy = {
      allowOnlyPolicyWifiNetworksToConnect: true,
    };
    await flushAsync();
    assertTrue(listItem.computeIsBlockedNetwork_());
  });

  test(
      'Show detail page when clicking on blocked cellular network item',
      async () => {
        init();

        // Set item to a policy blocked cellular network.
        const managedProperties = OncMojo.getDefaultManagedProperties(
            NetworkType.kCellular, 'cellular');
        managedProperties.connectionState = ConnectionStateType.kNotConnected;
        managedProperties.source = OncSource.kNone;
        mojoApi_.setManagedPropertiesForTest(managedProperties);

        const networkState =
            OncMojo.managedPropertiesToNetworkState(managedProperties);
        listItem.item = networkState;
        // Set global policy to restrict managed cellular networks.
        listItem.globalPolicy = {
          allowOnlyPolicyCellularNetworks: true,
        };
        await flushAsync();

        // Selecting the row should fire the show-detail event.
        const showDetailPromise = eventToPromise('selected', listItem);
        listItem.$.divOuter.click();
        const showDetailEvent = await showDetailPromise;
        assertEquals(showDetailEvent.detail, networkState);
      });

  [true, false].forEach(isUserLoggedIn => {
    test('pSIM Network unactivated', async () => {
      loadTimeData.overrideValues({
        'isUserLoggedIn': isUserLoggedIn,
      });
      init();

      const managedPropertiesNotActivated = OncMojo.getDefaultManagedProperties(
          NetworkType.kCellular, 'cellular');
      managedPropertiesNotActivated.typeProperties.cellular.iccid = '1000000';
      managedPropertiesNotActivated.typeProperties.cellular.activationState =
          ActivationStateType.kNotActivated;
      managedPropertiesNotActivated.typeProperties.cellular.paymentPortal = {
        url: 'url',
      };
      mojoApi_.setManagedPropertiesForTest(managedPropertiesNotActivated);

      const networkState = OncMojo.managedPropertiesToNetworkState(
          managedPropertiesNotActivated);
      listItem.item = networkState;

      await flushAsync();

      const sublabel = listItem.$.sublabel;
      const activateButton = listItem.$$('#activateButton');
      const arrow = listItem.$$('#subpageButton');
      assertTrue(!!arrow);

      if (isUserLoggedIn) {
        assertTrue(!!activateButton);
        assertTrue(sublabel.hidden);
      } else {
        assertFalse(!!activateButton);
        assertFalse(sublabel.hidden);
        assertEquals(
            listItem.i18n('networkListItemActivateAfterDeviceSetup'),
            sublabel.textContent.trim());
      }
    });
  });

  suite('Portal', function() {
    function initWithPortalState(portalState) {
      const managedProperties =
          OncMojo.getDefaultManagedProperties(NetworkType.kWiFi, 'wifiguid');
      managedProperties.source = OncSource.kUser;
      managedProperties.typeProperties.wifi.security = SecurityType.kNone;
      mojoApi_.setManagedPropertiesForTest(managedProperties);
      const networkState =
          OncMojo.managedPropertiesToNetworkState(managedProperties);
      networkState.portalState = portalState;
      networkState.connectionState = ConnectionStateType.kPortal;
      listItem.item = networkState;
      flush();
    }

    test('kPortal portalState show sign in description', async () => {
      init();
      initWithPortalState(PortalState.kPortal);
      const getSublabel = () => {
        const element = listItem.$$('#sublabel');
        return element ? element.textContent.trim() : '';
      };
      assertEquals(getSublabel(), listItem.i18n('networkListItemSignIn'));
      assertTrue(listItem.$$('#sublabel').classList.contains('warning'));
      assertFalse(!!listItem.$$('#sublabel').hasAttribute('active'));
    });

    test('kPortalSuspected portalState show sign in description', async () => {
      init();
      initWithPortalState(PortalState.kPortalSuspected);
      const getSublabel = () => {
        const element = listItem.$$('#sublabel');
        return element ? element.textContent.trim() : '';
      };
      assertEquals(getSublabel(), listItem.i18n('networkListItemSignIn'));
      assertTrue(listItem.$$('#sublabel').classList.contains('warning'));
      assertFalse(!!listItem.$$('#sublabel').hasAttribute('active'));
    });

    test(
        'kNoInternet portalState show no connectivity description',
        async () => {
          init();
          initWithPortalState(PortalState.kNoInternet);
          const getSublabel = () => {
            const element = listItem.$$('#sublabel');
            return element ? element.textContent.trim() : '';
          };
          assertEquals(
              getSublabel(),
              listItem.i18n('networkListItemConnectedNoConnectivity'));
          assertTrue(listItem.$$('#sublabel').classList.contains('warning'));
          assertFalse(!!listItem.$$('#sublabel').hasAttribute('active'));
        });
  });
});
