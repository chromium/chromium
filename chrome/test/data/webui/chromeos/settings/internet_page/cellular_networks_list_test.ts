// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CellularNetworksListElement, NetworkListElement} from 'chrome://os-settings/lazy_load.js';
import {LocalizedLinkElement, MultiDeviceBrowserProxyImpl, MultiDeviceFeatureState, PaperSpinnerLiteElement} from 'chrome://os-settings/os_settings.js';
import {CellularSetupPageName} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DeviceStateProperties, GlobalPolicy, InhibitReason, ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestMultideviceBrowserProxy} from '../multidevice_page/test_multidevice_browser_proxy.js';
import {clearBody} from '../utils.js';

suite('<cellular-networks-list>', () => {
  let cellularNetworkList: CellularNetworksListElement;
  let mojoApi: FakeNetworkConfig;
  let eSimManagerRemote: FakeESimManagerRemote;
  let browserProxy: TestMultideviceBrowserProxy;

  setup(() => {
    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  async function init() {
    clearBody();
    cellularNetworkList = document.createElement('cellular-networks-list');
    // iron-list will not create list items if the container of the list is of
    // size zero.
    cellularNetworkList.style.height = '100%';
    cellularNetworkList.style.width = '100%';
    document.body.appendChild(cellularNetworkList);
    await flushTasks();
  }

  function setCellularDeviceState(
      cellularDeviceState: Partial<DeviceStateProperties>) {
    cellularNetworkList.cellularDeviceState =
        cellularDeviceState as DeviceStateProperties;
  }

  function setGlobalPolicy(globalPolicy: Partial<GlobalPolicy>) {
    cellularNetworkList.globalPolicy = globalPolicy as GlobalPolicy;
  }

  function setManagedPropertiesForTest(
      type: NetworkType, properties: ManagedProperties[]) {
    mojoApi.resetForTest();
    mojoApi.setNetworkTypeEnabledState(type, true);
    const networks: OncMojo.NetworkStateProperties[] = [];
    properties.forEach((property) => {
      mojoApi.setManagedPropertiesForTest(property);
      networks.push(OncMojo.managedPropertiesToNetworkState(property));
    });
    cellularNetworkList.cellularDeviceState =
        mojoApi.getDeviceStateForTest(type)!;
    cellularNetworkList.networks = networks;
  }

  function initSimInfos() {
    const deviceState = cellularNetworkList.cellularDeviceState || {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    } as DeviceStateProperties;

    deviceState.simInfos ||= [];
    setCellularDeviceState(deviceState);
  }

  async function addPSimSlot() {
    initSimInfos();
    cellularNetworkList.push('cellularDeviceState.simInfos', {
      iccid: '',
    });
    await flushTasks();
  }

  async function addESimSlot() {
    initSimInfos();
    cellularNetworkList.push('cellularDeviceState.simInfos', {
      eid: 'eid',
    });
    await flushTasks();
  }

  async function clearSimSlots() {
    cellularNetworkList.set('cellularDeviceState.simInfos', []);
    await flushTasks();
  }

  function queryEsimNetworkList(): NetworkListElement|null {
    return cellularNetworkList.shadowRoot!.querySelector<NetworkListElement>(
        '#esimNetworkList');
  }

  function queryPsimNetworkList(): NetworkListElement|null {
    return cellularNetworkList.shadowRoot!.querySelector<NetworkListElement>(
        '#psimNetworkList');
  }

  function queryPsimNoNetworkFoundElement(): HTMLElement|null {
    return cellularNetworkList.shadowRoot!.querySelector<HTMLElement>(
        '#pSimNoNetworkFound');
  }

  function getNoEsimNetworksMessageWithLinkElement(): LocalizedLinkElement {
    const element =
        cellularNetworkList.shadowRoot!.querySelector<LocalizedLinkElement>(
            '#noEsimNetworksMessageWithLink');
    assertTrue(!!element);
    return element;
  }

  function getNoEsimFoundElement(): HTMLElement {
    const element = cellularNetworkList.shadowRoot!.querySelector<HTMLElement>(
        '#noEsimNetworksMessage');
    assertTrue(!!element);
    return element;
  }

  function getInhibitedSubtextElement(): HTMLElement {
    const element = cellularNetworkList.shadowRoot!.querySelector<HTMLElement>(
        '#inhibitedSubtext');
    assertTrue(!!element);
    return element;
  }

  function queryInhibitedSpinner(): PaperSpinnerLiteElement|null {
    return cellularNetworkList.shadowRoot!
        .querySelector<PaperSpinnerLiteElement>('#inhibitedSpinner');
  }

  [true, false].forEach((isInstantHotspotRebrandEnabled) => {
    test(
        `Tether, cellular and eSIM profiles instant hotspot rebrand enabled: ${
            isInstantHotspotRebrandEnabled}`,
        async () => {
          eSimManagerRemote.addEuiccForTest(2);
          loadTimeData.overrideValues({
            isInstantHotspotRebrandEnabled,
          });

          await init();
          browserProxy.setInstantTetheringStateForTest(
              MultiDeviceFeatureState.ENABLED_BY_USER);

          const eSimNetwork1 = OncMojo.getDefaultManagedProperties(
              NetworkType.kCellular, 'cellular_esim1', /*name=*/ 'foo');
          eSimNetwork1.typeProperties.cellular!.eid =
              '11111111111111111111111111111111';
          const eSimNetwork2 = OncMojo.getDefaultManagedProperties(
              NetworkType.kCellular, 'cellular_esim2', /*name=*/ 'foo');
          eSimNetwork2.typeProperties.cellular!.eid =
              '22222222222222222222222222222222';
          setManagedPropertiesForTest(NetworkType.kCellular, [
            OncMojo.getDefaultManagedProperties(
                NetworkType.kCellular, 'cellular1', /*name=*/ 'foo'),
            OncMojo.getDefaultManagedProperties(
                NetworkType.kCellular, 'cellular2', /*name=*/ 'foo'),
            eSimNetwork1,
            eSimNetwork2,
            OncMojo.getDefaultManagedProperties(
                NetworkType.kTether, 'tether1', /*name=*/ 'foo'),
            OncMojo.getDefaultManagedProperties(
                NetworkType.kTether, 'tether2', /*name=*/ 'foo'),
          ]);
          await addPSimSlot();
          await addESimSlot();

          const eSimNetworkList = queryEsimNetworkList();
          assertTrue(!!eSimNetworkList);

          const pSimNetworkList = queryPsimNetworkList();
          assertTrue(!!pSimNetworkList);

          assertEquals(2, eSimNetworkList.networks.length);
          assertEquals(2, pSimNetworkList.networks.length);
          assertEquals(0, eSimNetworkList.customItems.length);

          const tetherNetworkList =
              cellularNetworkList.shadowRoot!.querySelector<NetworkListElement>(
                  '#tetherNetworkList');

          if (isInstantHotspotRebrandEnabled) {
            assertNull(tetherNetworkList);
          } else {
            assertTrue(!!tetherNetworkList);
            assertEquals(2, tetherNetworkList.networks.length);
          }
        });
  });

  test(
      'Fire show cellular setup event on eSim no network link click',
      async () => {
        eSimManagerRemote.addEuiccForTest(0);
        await init();
        await addESimSlot();

        const noEsimNetworksMessageLink =
            getNoEsimNetworksMessageWithLinkElement().shadowRoot!.querySelector(
                'a');
        assertTrue(!!noEsimNetworksMessageLink);

        const showEsimCellularSetupPromise =
            eventToPromise('show-cellular-setup', cellularNetworkList);
        noEsimNetworksMessageLink.click();
        const eSimCellularEvent = await showEsimCellularSetupPromise;
        assertEquals(
            CellularSetupPageName.ESIM_FLOW_UI,
            eSimCellularEvent.detail.pageName,
        );
      });
    test(
        'Hide eSIM section when no EUICC is found or no eSIM slots',
        async () => {
          await init();

          const eSimNetwork = OncMojo.getDefaultManagedProperties(
              NetworkType.kCellular, 'cellular_esim', /*name=*/ 'foo');
          eSimNetwork.typeProperties.cellular!.eid =
              '11111111111111111111111111111111';
          eSimNetwork.typeProperties.cellular!.iccid =
              '11111111111111111111111111111111';
          setManagedPropertiesForTest(NetworkType.kCellular, [
            eSimNetwork,
            OncMojo.getDefaultManagedProperties(
                NetworkType.kTether, 'tether1', /*name=*/ 'foo'),
          ]);
          await flushTasks();

          // The list should be hidden with no EUICC or eSIM slots.
          assertNull(cellularNetworkList.shadowRoot!.querySelector(
              '#esimNetworkList'));

          // Add an eSIM slot.
          await addESimSlot();
          // The list should still be hidden.
          assertNull(queryEsimNetworkList());

          // Add an EUICC.
          eSimManagerRemote.addEuiccForTest(1);
          await flushTasks();
          // The list should now be showing
          assertTrue(!!queryEsimNetworkList());

          // Remove the eSIM slot
          await clearSimSlots();
          // The list should be hidden again.
          assertNull(queryEsimNetworkList());
        });

  test('Hide pSIM section when no pSIM slots', async () => {
    await init();
    setManagedPropertiesForTest(NetworkType.kCellular, [
      OncMojo.getDefaultManagedProperties(
          NetworkType.kTether, 'tether1', /*name=*/ 'foo'),
    ]);
    await flushTasks();
    assertNull(queryPsimNoNetworkFoundElement());

    await addPSimSlot();
    assertTrue(!!queryPsimNoNetworkFoundElement());

    await clearSimSlots();
    assertNull(queryPsimNoNetworkFoundElement());
  });

  test(
      'Show pSIM section when no pSIM slots but pSIM networks available',
      async () => {
        eSimManagerRemote.addEuiccForTest(2);
        await init();

        setManagedPropertiesForTest(NetworkType.kCellular, [
          OncMojo.getDefaultManagedProperties(
              NetworkType.kCellular, 'pSimCellular1', /*name=*/ 'foo'),
          OncMojo.getDefaultManagedProperties(
              NetworkType.kCellular, 'pSimcellular2', /*name=*/ 'foo'),
        ]);
        await flushTasks();

        const pSimNetworkList = queryPsimNetworkList();
        assertTrue(!!pSimNetworkList);
        assertEquals(2, pSimNetworkList.networks.length);
      });

  test('Hide instant tethering section when not enabled', async () => {
    // Tether networks should not be shown in the cellular network list when the
    // instant hotspot rebrand feature flag is enabled.
    loadTimeData.overrideValues({
      isInstantHotspotRebrandEnabled: false,
    });
    await init();
    assertNull(cellularNetworkList.shadowRoot!.querySelector(
        '#tetherNetworksNotSetup'));

    browserProxy.setInstantTetheringStateForTest(
        MultiDeviceFeatureState.ENABLED_BY_USER);
    await flushTasks();
    assertTrue(!!cellularNetworkList.shadowRoot!.querySelector(
        '#tetherNetworksNotSetup'));

    browserProxy.setInstantTetheringStateForTest(
        MultiDeviceFeatureState.UNAVAILABLE_NO_VERIFIED_HOST_NO_ELIGIBLE_HOST);
    await flushTasks();
    assertNull(cellularNetworkList.shadowRoot!.querySelector(
        '#tetherNetworksNotSetup'));
  });

  test('No network eSIM', async () => {
    eSimManagerRemote.addEuiccForTest(0);
    await init();
    setCellularDeviceState({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    });
    setGlobalPolicy({
      allowOnlyPolicyCellularNetworks: true,
    });
    await addESimSlot();

    const noEsimNetworksMessageElement =
        getNoEsimNetworksMessageWithLinkElement();
    assertTrue(noEsimNetworksMessageElement.hidden);

    const noEsimFoundMessageElement = getNoEsimFoundElement();
    assertFalse(noEsimFoundMessageElement.hidden);

    assertEquals(
        cellularNetworkList.i18n('eSimNetworkNotSetup'),
        noEsimFoundMessageElement.innerText.trim());

    setGlobalPolicy({
      allowOnlyPolicyCellularNetworks: false,
    });
    await flushTasks();

    assertFalse(noEsimNetworksMessageElement.hidden);
    assertTrue(noEsimFoundMessageElement.hidden);

    for (const inhibitReason
             of [InhibitReason.kNotInhibited, InhibitReason.kInstallingProfile,
                 InhibitReason.kRenamingProfile, InhibitReason.kRemovingProfile,
                 InhibitReason.kConnectingToProfile,
                 InhibitReason.kRefreshingProfileList,
                 InhibitReason.kResettingEuiccMemory,
                 InhibitReason.kDisablingProfile,
                 InhibitReason.kRequestingAvailableProfiles]) {
      cellularNetworkList.set(
          'cellularDeviceState.inhibitReason', inhibitReason);
      await flushTasks();

      const noEsimNetworksMessageWithLink =
          getNoEsimNetworksMessageWithLinkElement();
      const noEsimFoundMessage = getNoEsimFoundElement();

      if (inhibitReason === InhibitReason.kNotInhibited) {
        assertFalse(noEsimNetworksMessageWithLink.hidden);
        assertTrue(noEsimFoundMessage.hidden);
      } else if (
          inhibitReason === InhibitReason.kInstallingProfile ||
          inhibitReason === InhibitReason.kRefreshingProfileList ||
          inhibitReason === InhibitReason.kRequestingAvailableProfiles) {
        assertTrue(noEsimNetworksMessageWithLink.hidden);
        assertTrue(noEsimFoundMessage.hidden);
      } else {
        assertTrue(noEsimNetworksMessageWithLink.hidden);
        assertFalse(noEsimFoundMessage.hidden);
      }
    }
  });

  test('Fire show cellular setup event on add cellular clicked', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    await init();
    const eSimNetwork1 = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_esim1', /*name=*/ 'foo');
    eSimNetwork1.typeProperties.cellular!.eid =
        '11111111111111111111111111111111';
    eSimNetwork1.typeProperties.cellular!.iccid =
        '11111111111111111111111111111111';
    setManagedPropertiesForTest(NetworkType.kCellular, [
      OncMojo.getDefaultManagedProperties(
          NetworkType.kCellular, 'cellular1', /*name=*/ 'foo'),
      eSimNetwork1,
    ]);
    setCellularDeviceState({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    });
    setGlobalPolicy({
      allowOnlyPolicyCellularNetworks: true,
    });
    await addESimSlot();

    function getAddESimButton(): HTMLButtonElement {
      const buttonElement =
          cellularNetworkList.shadowRoot!.querySelector<HTMLButtonElement>(
              '#addESimButton');
      assertTrue(!!buttonElement);
      return buttonElement;
    }

    // When policy is enabled add cellular button should be disabled, and policy
    // indicator should be shown.
    const addESimButton = getAddESimButton();
    assertTrue(addESimButton.disabled);

    let policyIcon =
        cellularNetworkList.shadowRoot!.querySelector('cr-policy-indicator');
    assertTrue(!!policyIcon);
    assertFalse(policyIcon.hidden);

    setGlobalPolicy({
      allowOnlyPolicyCellularNetworks: false,
    });
    await flushTasks();

    assertFalse(addESimButton.disabled);
    policyIcon =
        cellularNetworkList.shadowRoot!.querySelector('cr-policy-indicator');
    assertTrue(!!policyIcon);
    assertTrue(policyIcon.hidden);

    // When device is inhibited add cellular button should be disabled.
    setCellularDeviceState({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kInstallingProfile,
    });
    await addESimSlot();

    assertTrue(addESimButton.disabled);

    // Device is not inhibited and policy is also false add cellular button
    // should be enabled
    setCellularDeviceState({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    });
    await addESimSlot();

    assertFalse(addESimButton.disabled);

    const showCellularSetupPromise =
        eventToPromise('show-cellular-setup', cellularNetworkList);
    addESimButton.click();
    await flushTasks();
    await showCellularSetupPromise;
  });

  test('Disable no esim link when cellular device is inhibited', async () => {
    eSimManagerRemote.addEuiccForTest(0);
    await init();
    setCellularDeviceState({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    });
    await addESimSlot();

    const noEsimNetworksMessageWithLink =
        getNoEsimNetworksMessageWithLinkElement();
    assertFalse(noEsimNetworksMessageWithLink.linkDisabled);

    setCellularDeviceState({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kResettingEuiccMemory,
    });
    await addESimSlot();

    assertTrue(noEsimNetworksMessageWithLink.linkDisabled);
  });

  test('Show inhibited subtext and spinner when inhibited', async () => {
    eSimManagerRemote.addEuiccForTest(0);
    await init();

    setCellularDeviceState({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kNotInhibited,
    });
    cellularNetworkList.canShowSpinner = true;
    await addESimSlot();

    const inhibitedSubtext = getInhibitedSubtextElement();
    assertTrue(inhibitedSubtext.hidden);

    const inhibtedSpinner = queryInhibitedSpinner();
    assertTrue(!!inhibtedSpinner);
    assertFalse(inhibtedSpinner.active);

    setCellularDeviceState({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kInstallingProfile,
    });
    await addESimSlot();

    assertFalse(inhibitedSubtext.hidden);
    assertTrue(inhibtedSpinner.active);

    // Do not show inihibited spinner if cellular setup dialog is open.
    cellularNetworkList.canShowSpinner = false;
    await flushTasks();
    assertNull(queryInhibitedSpinner());
  });

  test(
      'Inhibited subtext gets updated when inhibit reason changes',
      async () => {
        eSimManagerRemote.addEuiccForTest(0);
        await init();

        // Put the device under the inhibited status with kRefreshingProfileList
        // reason first.
        setCellularDeviceState({
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          inhibitReason: InhibitReason.kRefreshingProfileList,
        });
        cellularNetworkList.canShowSpinner = true;
        await addESimSlot();

        const inhibitedSubtext = getInhibitedSubtextElement();
        assertFalse(inhibitedSubtext.hidden);
        assertTrue(inhibitedSubtext.innerText.includes(cellularNetworkList.i18n(
            'cellularNetworRefreshingProfileListProfile')));

        const inhibitedSpinner = queryInhibitedSpinner();
        assertTrue(!!inhibitedSpinner);
        assertTrue(inhibitedSpinner.active);

        // When device inhibit reason changes from kRefreshingProfileList to
        // kInstallingProfile, the inhibited subtext should also get updated to
        // reflect that.
        setCellularDeviceState({
          inhibitReason: InhibitReason.kInstallingProfile,
        });
        await addESimSlot();

        assertFalse(inhibitedSubtext.hidden);
        assertTrue(inhibitedSubtext.innerText.includes(
            cellularNetworkList.i18n('cellularNetworkInstallingProfile')));
        assertTrue(inhibitedSpinner.active);
      });
});
