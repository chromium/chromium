// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CellularNetworksListElement, NetworkAlwaysOnVpnElement, NetworkListElement, SettingsInternetSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {ESimManagerRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {AlwaysOnVpnMode, InhibitReason, NetworkStateProperties, NetworkTypeStateProperties, ProxyMode, SIMInfo, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType, OncSource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';

suite('<settings-internet-subpage>', () => {
  let internetSubpage: SettingsInternetSubpageElement;
  let mojoApi: FakeNetworkConfig;
  let eSimManagerRemote: FakeESimManagerRemote;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      internetAddConnection: 'internetAddConnection',
      internetAddConnectionExpandA11yLabel:
          'internetAddConnectionExpandA11yLabel',
      internetAddConnectionNotAllowed: 'internetAddConnectionNotAllowed',
      internetAddThirdPartyVPN: 'internetAddThirdPartyVPN',
      internetAddVPN: 'internetAddVPN',
      internetAddWiFi: 'internetAddWiFi',
      internetDetailPageTitle: 'internetDetailPageTitle',
      internetKnownNetworksPageTitle: 'internetKnownNetworksPageTitle',
    });

    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(
        eSimManagerRemote as unknown as ESimManagerRemote);

    // Disable animations so sub-pages open within one event loop.
    disableAnimationsAndTransitions();
  });

  function setNetworksForTest(
      type: NetworkType, networks: NetworkStateProperties[]): void {
    mojoApi.resetForTest();
    mojoApi.setNetworkTypeEnabledState(type, true);
    mojoApi.addNetworksForTest(networks);
    internetSubpage.defaultNetwork = networks[0];
    internetSubpage.deviceState =
        mojoApi.getDeviceStateForTest(type) || undefined;
  }

  function createDeviceStateProps(
      type: NetworkType, deviceState: DeviceStateType,
      inhibitReason?: InhibitReason,
      simInfos?: SIMInfo[]): OncMojo.DeviceStateProperties {
    return {
      type,
      deviceState,
      inhibitReason: inhibitReason || InhibitReason.MIN_VALUE,
      simInfos: simInfos || undefined,
      ipv4Address: undefined,
      ipv6Address: undefined,
      imei: undefined,
      macAddress: undefined,
      scanning: false,
      simLockStatus: undefined,
      simAbsent: false,
      managedNetworkAvailable: false,
      serial: undefined,
      isCarrierLocked: false,
    };
  }

  /**
   *     @param networkList Networks to set. If left undefined, default networks
   * will be set.
   */
  function addCellularNetworks(networkList?: NetworkStateProperties[]): void {
    const networks = networkList || [
      OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular1'),
      OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
      OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether2'),
    ];

    mojoApi.setNetworkTypeEnabledState(NetworkType.kTether, false);
    setNetworksForTest(NetworkType.kCellular, networks);
    internetSubpage.tetherDeviceState =
        createDeviceStateProps(NetworkType.kTether, DeviceStateType.kEnabled);
    internetSubpage.deviceState =
        mojoApi.getDeviceStateForTest(NetworkType.kCellular) || undefined;
  }

  async function initSubpage(): Promise<void> {
    internetSubpage = document.createElement('settings-internet-subpage');
    mojoApi.resetForTest();
    eSimManagerRemote.addEuiccForTest(0);
    document.body.appendChild(internetSubpage);
    internetSubpage.init();
    await flushTasks();
  }

  teardown(() => {
    internetSubpage.remove();
    mojoApi.resetForTest();
    Router.getInstance().resetRouteForTesting();
  });

  suite('SubPage', () => {
    test('WiFi', async () => {
      await initSubpage();
      setNetworksForTest(NetworkType.kWiFi, [
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2'),
      ]);
      await flushTasks();
      assertEquals(2, internetSubpage.get('networkStateList_').length);
      const toggle =
          internetSubpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#deviceEnabledButton');
      assertTrue(!!toggle);
      assertFalse(toggle.disabled);
      const networkList =
          internetSubpage.shadowRoot!.querySelector<NetworkListElement>(
              '#networkList');
      assertTrue(!!networkList);
      assertEquals(2, networkList.networks.length);
    });

    test('Deep link to WiFi on/off toggle', async () => {
      await initSubpage();
      setNetworksForTest(NetworkType.kWiFi, [
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2'),
      ]);

      const WIFI_ON_OFF_SETTING = settingMojom.Setting.kWifiOnOff.toString();
      const params = new URLSearchParams();
      params.append('settingId', WIFI_ON_OFF_SETTING);
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
      await flushTasks();

      const deepLinkElement =
          internetSubpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Toggle WiFi should be focused for settingId=${
              WIFI_ON_OFF_SETTING}.`);
    });

    test('Tether', async () => {
      await initSubpage();
      setNetworksForTest(NetworkType.kTether, [
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether2'),
      ]);
      await flushTasks();
      assertEquals(2, internetSubpage.get('networkStateList_').length);
      const toggle =
          internetSubpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#deviceEnabledButton');
      assertTrue(!!toggle);
      assertFalse(toggle.disabled);
      const networkList =
          internetSubpage.shadowRoot!.querySelector<NetworkListElement>(
              '#networkList');
      assertTrue(!!networkList);
      assertEquals(2, networkList.networks.length);
      const tetherToggle =
          internetSubpage.shadowRoot!.querySelector('#tetherEnabledButton');
      // No separate tether toggle when Celular is not available; the
      // primary toggle enables or disables Tether in that case.
      assertNull(tetherToggle);
    });

    test('Deep link to tether on/off toggle w/o cellular', async () => {
      await initSubpage();
      setNetworksForTest(NetworkType.kTether, [
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether2'),
      ]);
      internetSubpage.tetherDeviceState =
          createDeviceStateProps(NetworkType.kTether, DeviceStateType.kEnabled);

      const INSTANT_TETHERING_ON_OFF_SETTING =
          settingMojom.Setting.kInstantTetheringOnOff.toString();
      const params = new URLSearchParams();
      params.append('settingId', INSTANT_TETHERING_ON_OFF_SETTING);
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
      await flushTasks();

      const deepLinkElement =
          internetSubpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Device enabled should be focused for settingId=${
              INSTANT_TETHERING_ON_OFF_SETTING}.`);
    });

    test('Deep link to add cellular button', async () => {
      await initSubpage();
      addCellularNetworks();
      await flushTasks();
      const cellularNetworkList =
          internetSubpage.shadowRoot!
              .querySelector<CellularNetworksListElement>(
                  '#cellularNetworkList');
      assertTrue(!!cellularNetworkList);

      const simInfos = [{
        eid: 'eid',
        iccid: '',
        isPrimary: true,
        slotId: 0,
      }];
      cellularNetworkList.cellularDeviceState = createDeviceStateProps(
          NetworkType.kCellular, DeviceStateType.kEnabled,
          InhibitReason.kNotInhibited, simInfos);

      cellularNetworkList.globalPolicy = {
        allowOnlyPolicyWifiNetworksToConnect: false,
        allowCellularSimLock: false,
        allowCellularHotspot: false,
        allowOnlyPolicyCellularNetworks: false,
        allowOnlyPolicyNetworksToAutoconnect: false,
        allowOnlyPolicyWifiNetworksToConnectIfAvailable: false,
        dnsQueriesMonitored: false,
        reportXdrEventsEnabled: false,
        blockedHexSsids: [],
        recommendedValuesAreEphemeral: false,
        userCreatedNetworkConfigurationsAreEphemeral: false,
      };
      await flushTasks();

      const ADD_ESIM_NETWORK_SETTING =
          settingMojom.Setting.kAddESimNetwork.toString();
      const params = new URLSearchParams();
      params.append('settingId', ADD_ESIM_NETWORK_SETTING);
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);

      await flushTasks();
      assertTrue(!!cellularNetworkList);

      const deepLinkElement = cellularNetworkList.getAddEsimButton();
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Add cellular button should be focused for settingId=${
              ADD_ESIM_NETWORK_SETTING}.`);
    });

    test('Tether plus Cellular', async () => {
      await initSubpage();
      addCellularNetworks();
      await flushTasks();
      assertEquals(3, internetSubpage.get('networkStateList_').length);
      const toggle =
          internetSubpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#deviceEnabledButton');
      assertTrue(!!toggle);
      assertFalse(toggle.disabled);
      const cellularNetworkList =
          internetSubpage.shadowRoot!
              .querySelector<CellularNetworksListElement>(
                  '#cellularNetworkList');
      assertTrue(!!cellularNetworkList);
      assertEquals(3, cellularNetworkList.networks.length);
      const tetherToggle =
          internetSubpage.shadowRoot!.querySelector('#tetherEnabledButton');
      assertNull(tetherToggle);
    });

    test('No js error when previous route is null', async () => {
      // This is a test for regression here https://crbug.com/1213162.
      // |oldRoute| in currentRouteChanged() could become undefined if a page
      // is refreshed. This test makes sure if |oldRoute| is undefined no js
      // console error is thrown.
      await initSubpage();
      addCellularNetworks();
      await flushTasks();
      const params = new URLSearchParams();
      params.append('guid', 'cellular1_guid');
      params.append('type', 'Cellular');
      params.append('name', 'cellular1');
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
      internetSubpage.currentRouteChanged(routes.INTERNET_NETWORKS, undefined);
    });

    // Regression test for https://crbug.com/1197342.
    test('pSIM section shows when cellularNetworks present', async () => {
      await initSubpage();

      const networks = [
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular1'),
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether2'),
      ];

      mojoApi.setNetworkTypeEnabledState(NetworkType.kTether, false);
      setNetworksForTest(NetworkType.kCellular, networks);
      internetSubpage.tetherDeviceState =
          createDeviceStateProps(NetworkType.kTether, DeviceStateType.kEnabled);
      const deviceState = mojoApi.getDeviceStateForTest(NetworkType.kCellular);
      assertTrue(!!deviceState);
      // This siminfo represents a pSIM slot because this it has no EID.
      deviceState.simInfos =
          [{iccid: '11111111111', isPrimary: true, slotId: 0, eid: ''}];
      internetSubpage.deviceState = deviceState;

      await flushTasks();
      const cellularNetworkList =
          internetSubpage.shadowRoot!.querySelector('#cellularNetworkList');
      assertTrue(!!cellularNetworkList);
      assertTrue(
          !!cellularNetworkList.shadowRoot!.querySelector('#psimNetworkList'));
    });

    // Regression test for https://crbug.com/1182406.
    test('Cellular subpage with no networks', async () => {
      await initSubpage();
      addCellularNetworks([] /* networkList */);
      await flushTasks();
      const cellularNetworkList =
          internetSubpage.shadowRoot!.querySelector('#cellularNetworkList');
      assertTrue(!!cellularNetworkList);
    });

    suite('VPN', () => {
      function addTestVpnProviders(): void {
        internetSubpage.vpnProviders = [
          {
            type: VpnType.kExtension,
            providerId: 'extension_id1',
            providerName: 'MyExtensionVPN1',
            appId: 'extension_id1',
            lastLaunchTime: {internalValue: 0n},
          },
          {
            type: VpnType.kExtension,
            providerId: 'extension_id2',
            providerName: 'MyExtensionVPN2',
            appId: 'extension_id2',
            lastLaunchTime: {internalValue: 0n},
          },
          {
            type: VpnType.kArc,
            providerId: 'vpn.app.package1',
            providerName: 'MyArcVPN1',
            appId: 'arcid1',
            lastLaunchTime: {internalValue: 1n},
          },
        ];
      }

      function addTestVpnNetworks(): void {
        const typeStateDefaultProps: NetworkTypeStateProperties = {
          vpn: undefined,
          cellular: undefined,
          ethernet: undefined,
          tether: undefined,
          wifi: undefined,
        };

        const defaultNetworkStateProps: NetworkStateProperties = {
          guid: '',
          name: '',
          type: NetworkType.kAll,
          connectionState: ConnectionStateType.kNotConnected,
          typeState: typeStateDefaultProps,
          connectable: true,
          connectRequested: false,
          errorState: undefined,
          portalProbeUrl: undefined,
          priority: 0,
          prohibitedByPolicy: false,
          portalState: PortalState.kUnknown,
          proxyMode: ProxyMode.kDirect,
          source: OncSource.kNone,
        };

        setNetworksForTest(NetworkType.kVPN, [
          OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn1'),
          OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn2'),
          {
            ...defaultNetworkStateProps,
            guid: 'extension1_vpn1_guid',
            name: 'vpn3',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kNotConnected,
            typeState: {
              ...typeStateDefaultProps,
              vpn: {
                type: VpnType.kExtension,
                providerId: 'extension_id1',
                providerName: 'MyExntensionVPN1',
              },
            },
          },
          {
            ...defaultNetworkStateProps,
            guid: 'extension1_vpn2_guid',
            name: 'vpn4',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kNotConnected,
            typeState: {
              ...typeStateDefaultProps,
              vpn: {
                type: VpnType.kExtension,
                providerId: 'extension_id1',
                providerName: 'MyExntensionVPN1',
              },
            },
          },
          {
            ...defaultNetworkStateProps,
            guid: 'extension2_vpn1_guid',
            name: 'vpn5',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kNotConnected,
            typeState: {
              ...typeStateDefaultProps,
              vpn: {
                type: VpnType.kExtension,
                providerId: 'extension_id2',
                providerName: 'MyExntensionVPN2',
              },
            },
          },
          {
            ...defaultNetworkStateProps,
            guid: 'arc_vpn1_guid',
            name: 'vpn6',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kConnected,
            typeState: {
              ...typeStateDefaultProps,
              vpn: {
                type: VpnType.kArc,
                providerId: 'vpn.app.package1',
                providerName: 'MyArcVPN1',
              },
            },
          },
          {
            ...defaultNetworkStateProps,
            guid: 'arc_vpn2_guid',
            name: 'vpn7',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kNotConnected,
            typeState: {
              ...typeStateDefaultProps,
              vpn: {
                type: VpnType.kArc,
                providerId: 'vpn.app.package1',
                providerName: 'MyArcVPN1',
              },
            },
          },
        ]);
      }

      function initVpn(): void {
        addTestVpnProviders();
        addTestVpnNetworks();
      }

      test('should update network state list properly', async () => {
        await initSubpage();
        initVpn();
        await flushTasks();
        const allNetworkLists =
            internetSubpage.shadowRoot!.querySelectorAll<NetworkListElement>(
                'network-list');
        // Built-in networks + 2 extension providers + 1 arc provider = 4
        assertEquals(4, allNetworkLists.length);
        // 2 built-in networks
        assertEquals(2, allNetworkLists[0]!.networks.length);
        // 2 networks with extension id 'extension_id1'
        assertEquals(2, allNetworkLists[1]!.networks.length);
        // 1 network with extension id 'extension_id2'
        assertEquals(1, allNetworkLists[2]!.networks.length);
        // 1 connected network with arc id 'vpn.app.package1'
        assertEquals(1, allNetworkLists[3]!.networks.length);
      });

      test(
          'should not show built-in VPN list when device is disabled',
          async () => {
            await initSubpage();
            initVpn();
            internetSubpage.deviceState = createDeviceStateProps(
                NetworkType.kVPN, DeviceStateType.kProhibited);

            await flushTasks();
            const allNetworkLists =
                internetSubpage.shadowRoot!
                    .querySelectorAll<NetworkListElement>('network-list');
            // 2 extension providers + 1 arc provider = 3
            // No built-in networks.
            assertEquals(3, allNetworkLists.length);
            // 2 networks with extension id 'extension_id1'
            assertEquals(2, allNetworkLists[0]!.networks.length);
            // 1 network with extension id 'extension_id2'
            assertEquals(1, allNetworkLists[1]!.networks.length);
            // 1 connected network with arc id 'vpn.app.package1'
            assertEquals(1, allNetworkLists[2]!.networks.length);
          });

      test('Always-on VPN settings reflects OFF mode', async () => {
        mojoApi.setAlwaysOnVpn({
          mode: AlwaysOnVpnMode.kOff,
          serviceGuid: '',
        });
        await initSubpage();
        initVpn();
        await flushTasks();
        const networkAlwaysOnVpn =
            internetSubpage.shadowRoot!
                .querySelector<NetworkAlwaysOnVpnElement>(
                    '#alwaysOnVpnSelector');
        assertTrue(!!networkAlwaysOnVpn);
        assertEquals(AlwaysOnVpnMode.kOff, networkAlwaysOnVpn.mode);
        assertEquals('', networkAlwaysOnVpn.service);
      });

      test('Always-on VPN settings reflects BEST-EFFORT mode', async () => {
        mojoApi.setAlwaysOnVpn({
          mode: AlwaysOnVpnMode.kBestEffort,
          serviceGuid: 'vpn1_guid',
        });
        await initSubpage();
        initVpn();
        await flushTasks();
        const networkAlwaysOnVpn =
            internetSubpage.shadowRoot!
                .querySelector<NetworkAlwaysOnVpnElement>(
                    '#alwaysOnVpnSelector');
        assertTrue(!!networkAlwaysOnVpn);
        assertEquals(AlwaysOnVpnMode.kBestEffort, networkAlwaysOnVpn.mode);
        assertEquals('vpn1_guid', networkAlwaysOnVpn.service);
      });

      test('Always-on VPN settings reflects STRICT mode', async () => {
        mojoApi.setAlwaysOnVpn({
          mode: AlwaysOnVpnMode.kStrict,
          serviceGuid: 'vpn2_guid',
        });
        await initSubpage();
        initVpn();
        await flushTasks();
        const networkAlwaysOnVpn =
            internetSubpage.shadowRoot!
                .querySelector<NetworkAlwaysOnVpnElement>(
                    '#alwaysOnVpnSelector');
        assertTrue(!!networkAlwaysOnVpn);
        assertEquals(AlwaysOnVpnMode.kStrict, networkAlwaysOnVpn.mode);
        assertEquals('vpn2_guid', networkAlwaysOnVpn.service);
      });

      test('Enabled always-on and select a service', async () => {
        await initSubpage();
        initVpn();
        await flushTasks();
        const networkAlwaysOnVpn =
            internetSubpage.shadowRoot!
                .querySelector<NetworkAlwaysOnVpnElement>(
                    '#alwaysOnVpnSelector');
        assertTrue(!!networkAlwaysOnVpn);
        networkAlwaysOnVpn.mode = AlwaysOnVpnMode.kBestEffort;
        networkAlwaysOnVpn.service = 'vpn1_guid';
        await flushTasks();
        const result_3 = await mojoApi.getAlwaysOnVpn();
        assertEquals(AlwaysOnVpnMode.kBestEffort, result_3.properties.mode);
        assertEquals('vpn1_guid', result_3.properties.serviceGuid);
      });

      test(
          'Enable always-on with STRICT mode and select a service',
          async () => {
            await initSubpage();
            initVpn();
            await flushTasks();
            const networkAlwaysOnVpn =
                internetSubpage.shadowRoot!
                    .querySelector<NetworkAlwaysOnVpnElement>(
                        '#alwaysOnVpnSelector');
            assertTrue(!!networkAlwaysOnVpn);
            networkAlwaysOnVpn.mode = AlwaysOnVpnMode.kStrict;
            networkAlwaysOnVpn.service = 'vpn2_guid';
            await flushTasks();
            const result_3 = await mojoApi.getAlwaysOnVpn();
            assertEquals(AlwaysOnVpnMode.kStrict, result_3.properties.mode);
            assertEquals('vpn2_guid', result_3.properties.serviceGuid);
          });

      test('Always-on VPN is not shown without networks', async () => {
        await initSubpage();
        const networkAlwaysOnVpn =
            internetSubpage.shadowRoot!.querySelector('#alwaysOnVpnSelector');
        assertNull(networkAlwaysOnVpn);
      });

      test('Always-on VPN list contains compatible networks', async () => {
        mojoApi.setAlwaysOnVpn({
          mode: AlwaysOnVpnMode.kBestEffort,
          serviceGuid: '',
        });
        await initSubpage();
        initVpn();
        await flushTasks();
        const networkAlwaysOnVpn =
            internetSubpage.shadowRoot!
                .querySelector<NetworkAlwaysOnVpnElement>(
                    '#alwaysOnVpnSelector');
        assertTrue(!!networkAlwaysOnVpn);
        // The list should contain 2 compatible networks.
        assertEquals(2, networkAlwaysOnVpn.networks.length);
      });
    });
  });
});
