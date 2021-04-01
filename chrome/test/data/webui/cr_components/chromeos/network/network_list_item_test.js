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
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {NetworkList} from 'chrome://resources/cr_components/chromeos/network/network_list_types.m.js';
// #import {keyDownOn, move} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// clang-format on

suite('NetworkListItemTest', function() {
  /** @type {!NetworkListItem|undefined} */
  let listItem;
  let mojom;
  let eSimManagerRemote = null;

  let eventTriggered;

  setup(function() {
    loadTimeData.overrideValues({
      updatedCellularActivationUi: true,
    });

    mojom = chromeos.networkConfig.mojom;
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
    const networkState =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kCellular);
    networkState.typeState.cellular.iccid = iccid;
    networkState.typeState.cellular.eid = eid;
    networkState.typeState.cellular.simLocked = simLocked;
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

    listItem.item = OncMojo.getDefaultNetworkState(mojom.NetworkType.kEthernet, 'eth0');

    // Update the network state.
    Polymer.dom.flush();

    // The network icon exists now.
    networkIcon = listItem.$$('network-icon');
    assertTrue(!!networkIcon);
  });

  test('Network provider name visibilty', async () => {
    listItem.item =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kEthernet, 'eth0');
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

        let spinner = listItem.$$('paper-spinner-lite');
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
    let arrow = listItem.$$('#subpage-button');
    assertTrue(!!arrow);

    assertFalse(!!unlockBtn);
    let networkStateText = listItem.$$('#networkStateText');
    assertTrue(!!networkStateText);
    assertNotEquals(
        networkStateLockedText, networkStateText.textContent.trim());

    listItem.set('networkState.typeState.cellular.simLocked', true);
    await flushAsync();
    // Arrow button should be hidden when unlock button is visible.
    arrow = listItem.$$('#subpage-button');
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
        listItem.networkState =
            OncMojo.getDefaultNetworkState(mojom.NetworkType.kCellular, name);
        listItem.deviceState = {
          type: mojom.NetworkType.kCellular,
          inhibitedReason: mojom.InhibitReason.kInstallingProfile,
        };
        await flushAsync();

        let arrow = listItem.$$('#subpage-button');
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
