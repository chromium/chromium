// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CrActionMenuElement, Router, routes, SettingsInternetDetailMenuElement} from 'chrome://os-settings/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {ESimManagerRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {InhibitReason, ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<settings-internet-detail-menu>', () => {
  let internetDetailMenu: SettingsInternetDetailMenuElement;
  let mojoApi: FakeNetworkConfig;
  let eSimManagerRemote: FakeESimManagerRemote;

  setup(() => {
    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);
    mojoApi.resetForTest();

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(
        eSimManagerRemote as unknown as ESimManagerRemote);

    mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
  });

  teardown(() => {
    internetDetailMenu.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function getManagedProperties(type: number, name: string): ManagedProperties {
    const result =
        OncMojo.getDefaultManagedProperties(type, name + '_guid', name);
    return result;
  }

  async function init(isGuestParam?: boolean): Promise<void> {
    const isGuest = !!isGuestParam;
    loadTimeData.overrideValues({isGuest});

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    internetDetailMenu =
        document.createElement('settings-internet-detail-menu');

    document.body.appendChild(internetDetailMenu);
    assertTrue(!!internetDetailMenu);
    await flushTasks();
  }

  async function addEsimCellularNetwork(
      iccid: string, eid: string, isManaged?: boolean): Promise<void> {
    const cellular = getManagedProperties(NetworkType.kCellular, 'cellular');
    cellular.typeProperties.cellular!.iccid = iccid;
    cellular.typeProperties.cellular!.eid = eid;
    if (isManaged) {
      cellular.source = OncSource.kDevicePolicy;
    }
    mojoApi.setManagedPropertiesForTest(cellular);
    await flushTasks();
  }

  /**
   * Asserts that current UI element with id |elementId| is focused
   * when |deepLinkId| is search params.
   */
  async function assertElementIsDeepLinked(
      deepLinkId: number, elementId: string): Promise<void> {
    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    params.append('settingId', deepLinkId.toString());
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    await waitAfterNextRender(internetDetailMenu);
    const actionMenu =
        internetDetailMenu.shadowRoot!.querySelector<CrActionMenuElement>(
            'cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);
    const deepLinkElement =
        actionMenu.querySelector<HTMLElement>(`#${elementId}`);
    assertTrue(!!deepLinkElement);

    await waitAfterNextRender(deepLinkElement);
    assertEquals(deepLinkElement, getDeepActiveElement());
  }

  test('Deep link to remove profile', async () => {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();
    await assertElementIsDeepLinked(27, 'removeBtn');
  });

  test('Deep link to rename profile', async () => {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();
    await assertElementIsDeepLinked(28, 'renameBtn');
  });

  test('Do not show triple dot when no iccid is present', async () => {
    addEsimCellularNetwork('', '11111111111111111111111111111111');
    await init();

    let tripleDot =
        internetDetailMenu.shadowRoot!.querySelector('#moreNetworkDetail');
    assertNull(tripleDot);

    addEsimCellularNetwork('100000', '11111111111111111111111111111111');

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    await flushTasks();
    tripleDot =
        internetDetailMenu.shadowRoot!.querySelector('#moreNetworkDetail');
    assertTrue(!!tripleDot);
  });

  test('Do not show triple dot when no eid is present', async () => {
    addEsimCellularNetwork('100000', '');
    await init();

    let tripleDot =
        internetDetailMenu.shadowRoot!.querySelector('#moreNetworkDetail');
    assertNull(tripleDot);

    addEsimCellularNetwork('100000', '11111111111111111111111111111111');

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    await flushTasks();
    tripleDot =
        internetDetailMenu.shadowRoot!.querySelector('#moreNetworkDetail');
    assertTrue(!!tripleDot);
  });

  test('Do not show triple dot menu in guest mode', async () => {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init(/*isGuestParam=*/ true);

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);
    await flushTasks();

    // Has ICCID and EID, but not shown since the user is in guest mode.
    assertNull(
        internetDetailMenu.shadowRoot!.querySelector('#moreNetworkDetail'));
  });

  test('Rename menu click', async () => {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    await flushTasks();
    const tripleDot =
        internetDetailMenu.shadowRoot!.querySelector<HTMLButtonElement>(
            '#moreNetworkDetail');
    assertTrue(!!tripleDot);

    tripleDot.click();
    await flushTasks();

    const actionMenu =
        internetDetailMenu.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    const renameBtn = actionMenu.querySelector<HTMLButtonElement>('#renameBtn');
    assertTrue(!!renameBtn);

    const renameProfilePromise =
        eventToPromise('show-esim-profile-rename-dialog', internetDetailMenu);
    renameBtn.click();
    await Promise.all([renameProfilePromise, flushTasks()]);

    assertFalse(actionMenu.open);
  });

  test('Remove menu button click', async () => {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    await flushTasks();
    const tripleDot =
        internetDetailMenu.shadowRoot!.querySelector<HTMLButtonElement>(
            '#moreNetworkDetail');
    assertTrue(!!tripleDot);

    tripleDot.click();
    await flushTasks();

    const actionMenu =
        internetDetailMenu.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    const removeBtn = actionMenu.querySelector<HTMLButtonElement>('#removeBtn');
    assertTrue(!!removeBtn);

    const removeProfilePromise =
        eventToPromise('show-esim-remove-profile-dialog', internetDetailMenu);
    removeBtn.click();
    await Promise.all([removeProfilePromise, flushTasks()]);

    assertFalse(actionMenu.open);
  });

  test('Menu is disabled when inhibited', async () => {
    addEsimCellularNetwork('100000', '11111111111111111111111111111111');
    await init();

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    await flushTasks();
    const tripleDot =
        internetDetailMenu.shadowRoot!.querySelector<HTMLButtonElement>(
            '#moreNetworkDetail');
    assertTrue(!!tripleDot);
    assertFalse(tripleDot.disabled);

    internetDetailMenu.deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      inhibitReason: InhibitReason.kConnectingToProfile,
      ipv4Address: undefined,
      ipv6Address: undefined,
      imei: '',
      macAddress: '',
      scanning: false,
      simLockStatus: undefined,
      simInfos: [],
      simAbsent: false,
      managedNetworkAvailable: false,
      serial: '',
      isCarrierLocked: false,
      isFlashing: false,
    };
    assertTrue(tripleDot.disabled);

    internetDetailMenu.deviceState = {
      ...internetDetailMenu.deviceState,
      inhibitReason: InhibitReason.kNotInhibited,
    };
    assertFalse(tripleDot.disabled);
  });

  test('Menu is disabled on managed profile', async () => {
    addEsimCellularNetwork(
        '100000', '11111111111111111111111111111111', /*isManaged=*/ true);
    await init();

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    await flushTasks();
    const tripleDot =
        internetDetailMenu.shadowRoot!.querySelector<HTMLButtonElement>(
            '#moreNetworkDetail');
    assertTrue(!!tripleDot);
    assertTrue(tripleDot.disabled);
  });

  test(
      'Esim profile name is updated when value changes in eSIM manager',
      async () => {
        const profileName = 'test profile name';
        const iccid = '100000';
        const eid = '1111111111';

        addEsimCellularNetwork(iccid, eid);
        await init();
        await flushTasks();
        const tripleDot =
            internetDetailMenu.shadowRoot!.querySelector<HTMLButtonElement>(
                '#moreNetworkDetail');
        assertTrue(!!tripleDot);
        assertFalse(tripleDot.disabled);

        // Change esim profile name.
        const cellular =
            getManagedProperties(NetworkType.kCellular, 'cellular');
        cellular.typeProperties.cellular!.iccid = iccid;
        cellular.typeProperties.cellular!.eid = eid;
        cellular.name!.activeValue = profileName;
        mojoApi.setManagedPropertiesForTest(cellular);
        await flushTasks();

        // Trigger change in esim manager listener
        eSimManagerRemote.notifyProfileChangedForTest(null);
        await flushTasks();

        tripleDot.click();
        await flushTasks();

        const actionMenu =
            internetDetailMenu.shadowRoot!.querySelector('cr-action-menu');
        assertTrue(!!actionMenu);
        assertTrue(actionMenu.open);

        const renameBtn =
            actionMenu.querySelector<HTMLButtonElement>('#renameBtn');
        assertTrue(!!renameBtn);

        const renameProfilePromise = eventToPromise(
            'show-esim-profile-rename-dialog', internetDetailMenu);
        renameBtn.click();
        const event = await renameProfilePromise;
        assertEquals(profileName, event.detail.networkState.name);
      });

  test('Network state is null if no profile is found', async () => {
    const getTrippleDot = () => {
      return internetDetailMenu.shadowRoot!.querySelector('#moreNetworkDetail');
    };
    addEsimCellularNetwork('1', '1');
    await init();
    assertTrue(!!getTrippleDot());

    // Remove current eSIM profile.
    mojoApi.resetForTest();
    await flushTasks();

    // Trigger change in esim manager listener
    eSimManagerRemote.notifyProfileChangedForTest(null);
    await flushTasks();

    assertNull(getTrippleDot());
  });
});
