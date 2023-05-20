// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {CellularSetupRemote} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-webui.js';
import {AlwaysOnVpnMode, CrosNetworkConfigRemote, InhibitReason, NetworkStateProperties, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('InternetSubpage', function() {
  /** @type {?SettingsInternetSubpageElement} */
  let internetSubpage = null;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  /** @type {?CellularSetupRemote} */
  let eSimManagerRemote;

  suiteSetup(function() {
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

    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;

    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function setNetworksForTest(type, networks) {
    mojoApi_.resetForTest();
    mojoApi_.setNetworkTypeEnabledState(type, true);
    mojoApi_.addNetworksForTest(networks);
    internetSubpage.defaultNetwork = networks[0];
    internetSubpage.deviceState = mojoApi_.getDeviceStateForTest(type);
  }

  /**
   * @param {!Array<!NetworkStateProperties>=}
   *     opt_networks Networks to set. If left undefined, default networks will
   *     be set.
   */
  function addCellularNetworks(opt_networks) {
    const networks = opt_networks || [
      OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular1'),
      OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
      OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether2'),
    ];

    mojoApi_.setNetworkTypeEnabledState(NetworkType.kTether);
    setNetworksForTest(NetworkType.kCellular, networks);
    internetSubpage.tetherDeviceState = {
      type: NetworkType.kTether,
      deviceState: DeviceStateType.kEnabled,
    };
    internetSubpage.cellularDeviceState =
        mojoApi_.getDeviceStateForTest(NetworkType.kCellular);
  }

  function initSubpage() {
    PolymerTest.clearBody();
    internetSubpage = document.createElement('settings-internet-subpage');
    assertTrue(!!internetSubpage);
    mojoApi_.resetForTest();
    eSimManagerRemote.addEuiccForTest(0);
    document.body.appendChild(internetSubpage);
    internetSubpage.init();
    return flushAsync();
  }

  teardown(function() {
    internetSubpage.remove();
    internetSubpage = null;
    Router.getInstance().resetRouteForTesting();
  });

  suite('SubPage', function() {
    test('WiFi', function() {
      initSubpage();
      setNetworksForTest(NetworkType.kWiFi, [
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2'),
      ]);
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const toggle =
            internetSubpage.shadowRoot.querySelector('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList =
            internetSubpage.shadowRoot.querySelector('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
      });
    });

    test('Deep link to WiFi on/off toggle', async () => {
      initSubpage();
      setNetworksForTest(NetworkType.kWiFi, [
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2'),
      ]);

      const params = new URLSearchParams();
      params.append('settingId', '4');
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);

      await flushAsync();

      const deepLinkElement =
          internetSubpage.shadowRoot.querySelector('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Toggle WiFi should be focused for settingId=4.');
    });

    test('Tether', function() {
      initSubpage();
      setNetworksForTest(NetworkType.kTether, [
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether2'),
      ]);
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const toggle =
            internetSubpage.shadowRoot.querySelector('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList =
            internetSubpage.shadowRoot.querySelector('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
        const tetherToggle =
            internetSubpage.shadowRoot.querySelector('#tetherEnabledButton');
        // No separate tether toggle when Celular is not available; the
        // primary toggle enables or disables Tether in that case.
        assertFalse(!!tetherToggle);
      });
    });

    test('Deep link to tether on/off toggle w/o cellular', async () => {
      initSubpage();
      setNetworksForTest(NetworkType.kTether, [
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether2'),
      ]);
      internetSubpage.tetherDeviceState = {
        type: NetworkType.kTether,
        deviceState: DeviceStateType.kEnabled,
      };

      const params = new URLSearchParams();
      params.append('settingId', '22');
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);

      await flushAsync();

      const deepLinkElement =
          internetSubpage.shadowRoot.querySelector('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Device enabled should be focused for settingId=22.');
    });

    test('Deep link to add cellular button', async () => {
      initSubpage();
      addCellularNetworks();
      await flushAsync();
      const cellularNetworkList =
          internetSubpage.shadowRoot.querySelector('#cellularNetworkList');
      cellularNetworkList.cellularDeviceState = {
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        inhibitReason: InhibitReason.kNotInhibited,
        simInfos: [{eid: 'eid'}],
      };
      cellularNetworkList.globalPolicy = {
        allowOnlyPolicyWifiNetworksToConnect: false,
      };
      await flushAsync();

      const params = new URLSearchParams();
      params.append('settingId', '26');
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);

      await flushAsync();
      assertTrue(!!cellularNetworkList);

      const deepLinkElement = cellularNetworkList.getAddEsimButton();
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Add cellular button should be focused for settingId=26.');
    });

    test(
        'Tether plus Cellular', function() {
          initSubpage();
          addCellularNetworks();
          return flushAsync().then(() => {
            assertEquals(3, internetSubpage.networkStateList_.length);
            const toggle = internetSubpage.shadowRoot.querySelector(
                '#deviceEnabledButton');
            assertTrue(!!toggle);
            assertFalse(toggle.disabled);
            const cellularNetworkList =
                internetSubpage.shadowRoot.querySelector(
                    '#cellularNetworkList');
            assertTrue(!!cellularNetworkList);
            assertEquals(3, cellularNetworkList.networks.length);
            const tetherToggle = internetSubpage.shadowRoot.querySelector(
                '#tetherEnabledButton');
            assertFalse(!!tetherToggle);
          });
        });

    test('No js error when previous route is null', function() {
      // This is a test for regression here https://crbug.com/1213162.
      // |oldRoute| in currentRouteChanged() could become undefined if a page
      // is refreshed. This test makes sure if |oldRoute| is undefined no js
      // console error is thrown.
      initSubpage();
      addCellularNetworks();
      return flushAsync().then(() => {
        const params = new URLSearchParams();
        params.append('guid', 'cellular1_guid');
        params.append('type', 'Cellular');
        params.append('name', 'cellular1');
        Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
        internetSubpage.currentRouteChanged(
            routes.INTERNET_NETWORKS, undefined);
      });
    });

    // Regression test for https://crbug.com/1197342.
    test('pSIM section shows when cellularNetworks present', async () => {
      initSubpage();

      const networks = [
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular1'),
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether2'),
      ];

      mojoApi_.setNetworkTypeEnabledState(NetworkType.kTether);
      setNetworksForTest(NetworkType.kCellular, networks);
      internetSubpage.tetherDeviceState = {
        type: NetworkType.kTether,
        deviceState: DeviceStateType.kEnabled,
      };
      const deviceState = mojoApi_.getDeviceStateForTest(NetworkType.kCellular);
      // This siminfo represents a pSIM slot because this it has no EID.
      deviceState.simInfos = [{
        iccid: '11111111111',
        isPrimary: true,
      }];
      internetSubpage.deviceState = deviceState;

      await flushAsync();
      const cellularNetworkList =
          internetSubpage.shadowRoot.querySelector('#cellularNetworkList');
      assertTrue(
          !!cellularNetworkList.shadowRoot.querySelector('#psimNetworkList'));
    });

    // Regression test for https://crbug.com/1182406.
    test(
        'Cellular subpage with no networks', function() {
          initSubpage();
          addCellularNetworks([] /* networks */);
          return flushAsync().then(() => {
            const cellularNetworkList =
                internetSubpage.shadowRoot.querySelector(
                    '#cellularNetworkList');
            assertTrue(!!cellularNetworkList);
          });
        });

    suite('VPN', function() {
      function initVpn() {
        addTestVpnProviders();
        addTestVpnNetworks();
      }

      function addTestVpnProviders() {
        internetSubpage.vpnProviders = [
          {
            type: VpnType.kExtension,
            providerId: 'extension_id1',
            providerName: 'MyExtensionVPN1',
            appId: 'extension_id1',
            lastLaunchTime: {internalValue: 0},
          },
          {
            type: VpnType.kExtension,
            providerId: 'extension_id2',
            providerName: 'MyExtensionVPN2',
            appId: 'extension_id2',
            lastLaunchTime: {internalValue: 0},
          },
          {
            type: VpnType.kArc,
            providerId: 'vpn.app.package1',
            providerName: 'MyArcVPN1',
            appId: 'arcid1',
            lastLaunchTime: {internalValue: 1},
          },
        ];
      }

      function addTestVpnNetworks() {
        setNetworksForTest(NetworkType.kVPN, [
          OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn1'),
          OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn2'),
          {
            guid: 'extension1_vpn1_guid',
            name: 'vpn3',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: VpnType.kExtension,
                providerId: 'extension_id1',
                providerName: 'MyExntensionVPN1',
              },
            },
          },
          {
            guid: 'extension1_vpn2_guid',
            name: 'vpn4',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: VpnType.kExtension,
                providerId: 'extension_id1',
                providerName: 'MyExntensionVPN1',
              },
            },
          },
          {
            guid: 'extension2_vpn1_guid',
            name: 'vpn5',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: VpnType.kExtension,
                providerId: 'extension_id2',
                providerName: 'MyExntensionVPN2',
              },
            },
          },
          {
            guid: 'arc_vpn1_guid',
            name: 'vpn6',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kConnected,
            typeState: {
              vpn: {
                type: VpnType.kArc,
                providerId: 'vpn.app.package1',
                providerName: 'MyArcVPN1',
              },
            },
          },
          {
            guid: 'arc_vpn2_guid',
            name: 'vpn7',
            type: NetworkType.kVPN,
            connectionState: ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: VpnType.kArc,
                providerId: 'vpn.app.package1',
                providerName: 'MyArcVPN1',
              },
            },
          },
        ]);
      }

      test('should update network state list properly', function() {
        initSubpage();
        initVpn();
        return flushAsync().then(() => {
          const allNetworkLists =
              internetSubpage.shadowRoot.querySelectorAll('network-list');
          // Built-in networks + 2 extension providers + 1 arc provider = 4
          assertEquals(4, allNetworkLists.length);
          // 2 built-in networks
          assertEquals(2, allNetworkLists[0].networks.length);
          // 2 networks with extension id 'extension_id1'
          assertEquals(2, allNetworkLists[1].networks.length);
          // 1 network with extension id 'extension_id2'
          assertEquals(1, allNetworkLists[2].networks.length);
          // 1 connected network with arc id 'vpn.app.package1'
          assertEquals(1, allNetworkLists[3].networks.length);
        });
      });

      test(
          'should not show built-in VPN list when device is disabled',
          function() {
            initSubpage();
            initVpn();
            internetSubpage.deviceState = {
              type: NetworkType.kVPN,
              deviceState: DeviceStateType.kProhibited,
            };

            return flushAsync().then(() => {
              const allNetworkLists =
                  internetSubpage.shadowRoot.querySelectorAll('network-list');
              // 2 extension providers + 1 arc provider = 3
              // No built-in networks.
              assertEquals(3, allNetworkLists.length);
              // 2 networks with extension id 'extension_id1'
              assertEquals(2, allNetworkLists[0].networks.length);
              // 1 network with extension id 'extension_id2'
              assertEquals(1, allNetworkLists[1].networks.length);
              // 1 connected network with arc id 'vpn.app.package1'
              assertEquals(1, allNetworkLists[2].networks.length);
            });
          });

      test('Always-on VPN settings reflects OFF mode', () => {
        mojoApi_.setAlwaysOnVpn({
          mode: AlwaysOnVpnMode.kOff,
          serviceGuid: '',
        });
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.shadowRoot.querySelector(
                      '#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              assertEquals(AlwaysOnVpnMode.kOff, networkAlwaysOnVpn.mode);
              assertEquals('', networkAlwaysOnVpn.service);
            });
      });

      test('Always-on VPN settings reflects BEST-EFFORT mode', () => {
        mojoApi_.setAlwaysOnVpn({
          mode: AlwaysOnVpnMode.kBestEffort,
          serviceGuid: 'vpn1_guid',
        });
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.shadowRoot.querySelector(
                      '#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              assertEquals(
                  AlwaysOnVpnMode.kBestEffort, networkAlwaysOnVpn.mode);
              assertEquals('vpn1_guid', networkAlwaysOnVpn.service);
            });
      });

      test('Always-on VPN settings reflects STRICT mode', () => {
        mojoApi_.setAlwaysOnVpn({
          mode: AlwaysOnVpnMode.kStrict,
          serviceGuid: 'vpn2_guid',
        });
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.shadowRoot.querySelector(
                      '#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              assertEquals(AlwaysOnVpnMode.kStrict, networkAlwaysOnVpn.mode);
              assertEquals('vpn2_guid', networkAlwaysOnVpn.service);
            });
      });

      test('Enabled always-on and select a service', () => {
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.shadowRoot.querySelector(
                      '#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              networkAlwaysOnVpn.mode = AlwaysOnVpnMode.kBestEffort;
              networkAlwaysOnVpn.service = 'vpn1_guid';
              return flushAsync();
            })
            .then(() => mojoApi_.getAlwaysOnVpn())
            .then(result => {
              assertEquals(AlwaysOnVpnMode.kBestEffort, result.properties.mode);
              assertEquals('vpn1_guid', result.properties.serviceGuid);
            });
      });

      test('Enable always-on with STRICT mode and select a service', () => {
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.shadowRoot.querySelector(
                      '#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              networkAlwaysOnVpn.mode = AlwaysOnVpnMode.kStrict;
              networkAlwaysOnVpn.service = 'vpn2_guid';
              return flushAsync();
            })
            .then(() => mojoApi_.getAlwaysOnVpn())
            .then(result => {
              assertEquals(AlwaysOnVpnMode.kStrict, result.properties.mode);
              assertEquals('vpn2_guid', result.properties.serviceGuid);
            });
      });

      test('Always-on VPN is not shown without networks', () => {
        return initSubpage().then(() => {
          const networkAlwaysOnVpn =
              internetSubpage.shadowRoot.querySelector('#alwaysOnVpnSelector');
          assert(!networkAlwaysOnVpn);
        });
      });

      test('Always-on VPN list contains compatible networks', () => {
        mojoApi_.setAlwaysOnVpn({
          mode: AlwaysOnVpnMode.kBestEffort,
          serviceGuid: '',
        });
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.shadowRoot.querySelector(
                      '#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              // The list should contain 2 compatible networks.
              assertEquals(2, networkAlwaysOnVpn.networks.length);
            });
      });
    });
  });
});
