// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

suite('InternetSubpage', function() {
  /** @type {?SettingsInternetSubpageElement} */
  let internetSubpage = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  /** @type {?ash.cellularSetup.mojom.CellularSetupRemote} */
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
   * @param {!Array<!chromeos.networkConfig.mojom.NetworkStateProperties>=}
   *     opt_networks Networks to set. If left undefined, default networks will
   *     be set.
   */
  function addCellularNetworks(opt_networks) {
    const mojom = chromeos.networkConfig.mojom;

    const networks = opt_networks || [
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kCellular, 'cellular1'),
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
      OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
    ];

    mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kTether);
    setNetworksForTest(mojom.NetworkType.kCellular, networks);
    internetSubpage.tetherDeviceState = {
      type: mojom.NetworkType.kTether,
      deviceState: mojom.DeviceStateType.kEnabled
    };
    internetSubpage.cellularDeviceState =
        mojoApi_.getDeviceStateForTest(mojom.NetworkType.kCellular);
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
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest(mojom.NetworkType.kWiFi, [
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi2'),
      ]);
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const toggle = internetSubpage.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = internetSubpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
      });
    });

    test('Deep link to WiFi on/off toggle', async () => {
      initSubpage();
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest(mojom.NetworkType.kWiFi, [
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi2'),
      ]);

      const params = new URLSearchParams();
      params.append('settingId', '4');
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);

      await flushAsync();

      const deepLinkElement = internetSubpage.$$('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Toggle WiFi should be focused for settingId=4.');
    });

    test('Tether', function() {
      initSubpage();
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest(mojom.NetworkType.kTether, [
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
      ]);
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const toggle = internetSubpage.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = internetSubpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
        const tetherToggle = internetSubpage.$$('#tetherEnabledButton');
        // No separate tether toggle when Celular is not available; the
        // primary toggle enables or disables Tether in that case.
        assertFalse(!!tetherToggle);
      });
    });

    test('Deep link to tether on/off toggle w/o cellular', async () => {
      initSubpage();
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest(mojom.NetworkType.kTether, [
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
      ]);
      internetSubpage.tetherDeviceState = {
        type: mojom.NetworkType.kTether,
        deviceState: mojom.DeviceStateType.kEnabled
      };

      const params = new URLSearchParams();
      params.append('settingId', '22');
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);

      await flushAsync();

      const deepLinkElement = internetSubpage.$$('#deviceEnabledButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Device enabled should be focused for settingId=22.');
    });

    test('Deep link to add cellular button', async () => {
      initSubpage();
      addCellularNetworks();
      const mojom = chromeos.networkConfig.mojom;
      await flushAsync();
      const cellularNetworkList = internetSubpage.$$('#cellularNetworkList');
      cellularNetworkList.cellularDeviceState = {
        type: mojom.NetworkType.kCellular,
        deviceState: mojom.DeviceStateType.kEnabled,
        inhibitReason: mojom.InhibitReason.kNotInhibited,
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
          const mojom = chromeos.networkConfig.mojom;
          addCellularNetworks();
          return flushAsync().then(() => {
            assertEquals(3, internetSubpage.networkStateList_.length);
            const toggle = internetSubpage.$$('#deviceEnabledButton');
            assertTrue(!!toggle);
            assertFalse(toggle.disabled);
            const cellularNetworkList =
                internetSubpage.$$('#cellularNetworkList');
            assertTrue(!!cellularNetworkList);
            assertEquals(3, cellularNetworkList.networks.length);
            const tetherToggle = internetSubpage.$$('#tetherEnabledButton');
            assertFalse(!!tetherToggle);
          });
        });

    test('No js error when previous route is null', function() {
      // This is a test for regression here https://crbug.com/1213162.
      // |oldRoute| in currentRouteChanged() could become undefined if a page
      // is refreshed. This test makes sure if |oldRoute| is undefined no js
      // console error is thrown.
      initSubpage();
      const mojom = chromeos.networkConfig.mojom;
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

      const mojom = chromeos.networkConfig.mojom;
      const networks = [
        OncMojo.getDefaultNetworkState(
            mojom.NetworkType.kCellular, 'cellular1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
      ];

      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kTether);
      setNetworksForTest(mojom.NetworkType.kCellular, networks);
      internetSubpage.tetherDeviceState = {
        type: mojom.NetworkType.kTether,
        deviceState: mojom.DeviceStateType.kEnabled
      };
      const deviceState =
          mojoApi_.getDeviceStateForTest(mojom.NetworkType.kCellular);
      // This siminfo represents a pSIM slot because this it has no EID.
      deviceState.simInfos = [{
        iccid: '11111111111',
        isPrimary: true,
      }];
      internetSubpage.deviceState = deviceState;

      await flushAsync();
      const cellularNetworkList = internetSubpage.$$('#cellularNetworkList');
      assertTrue(!!cellularNetworkList.$$('#psimNetworkList'));
    });

    // Regression test for https://crbug.com/1182406.
    test(
        'Cellular subpage with no networks', function() {
          initSubpage();
          addCellularNetworks([] /* networks */);
          return flushAsync().then(() => {
            const cellularNetworkList =
                internetSubpage.$$('#cellularNetworkList');
            assertTrue(!!cellularNetworkList);
          });
        });

    suite('VPN', function() {
      function initVpn() {
        addTestVpnProviders();
        addTestVpnNetworks();
      }

      function addTestVpnProviders() {
        const mojom = chromeos.networkConfig.mojom;
        internetSubpage.vpnProviders = [
          {
            type: mojom.VpnType.kExtension,
            providerId: 'extension_id1',
            providerName: 'MyExtensionVPN1',
            appId: 'extension_id1',
            lastLaunchTime: {internalValue: 0},
          },
          {
            type: mojom.VpnType.kExtension,
            providerId: 'extension_id2',
            providerName: 'MyExtensionVPN2',
            appId: 'extension_id2',
            lastLaunchTime: {internalValue: 0},
          },
          {
            type: mojom.VpnType.kArc,
            providerId: 'vpn.app.package1',
            providerName: 'MyArcVPN1',
            appId: 'arcid1',
            lastLaunchTime: {internalValue: 1},
          },
        ];
      }

      function addTestVpnNetworks() {
        const mojom = chromeos.networkConfig.mojom;
        setNetworksForTest(mojom.NetworkType.kVPN, [
          OncMojo.getDefaultNetworkState(mojom.NetworkType.kVPN, 'vpn1'),
          OncMojo.getDefaultNetworkState(mojom.NetworkType.kVPN, 'vpn2'),
          {
            guid: 'extension1_vpn1_guid',
            name: 'vpn3',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kExtension,
                providerId: 'extension_id1',
                providerName: 'MyExntensionVPN1',
              }
            }
          },
          {
            guid: 'extension1_vpn2_guid',
            name: 'vpn4',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kExtension,
                providerId: 'extension_id1',
                providerName: 'MyExntensionVPN1',
              }
            }
          },
          {
            guid: 'extension2_vpn1_guid',
            name: 'vpn5',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kExtension,
                providerId: 'extension_id2',
                providerName: 'MyExntensionVPN2',
              }
            }
          },
          {
            guid: 'arc_vpn1_guid',
            name: 'vpn6',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kArc,
                providerId: 'vpn.app.package1',
                providerName: 'MyArcVPN1',
              }
            }
          },
          {
            guid: 'arc_vpn2_guid',
            name: 'vpn7',
            type: mojom.NetworkType.kVPN,
            connectionState: mojom.ConnectionStateType.kNotConnected,
            typeState: {
              vpn: {
                type: mojom.VpnType.kArc,
                providerId: 'vpn.app.package1',
                providerName: 'MyArcVPN1',
              }
            }
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
            const mojom = chromeos.networkConfig.mojom;
            internetSubpage.deviceState = {
              type: mojom.NetworkType.kVPN,
              deviceState: mojom.DeviceStateType.kProhibited
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
        const mojom = chromeos.networkConfig.mojom;
        mojoApi_.setAlwaysOnVpn({
          mode: mojom.AlwaysOnVpnMode.kOff,
          serviceGuid: '',
        });
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.$$('#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              assertEquals(mojom.AlwaysOnVpnMode.kOff, networkAlwaysOnVpn.mode);
              assertEquals('', networkAlwaysOnVpn.service);
            });
      });

      test('Always-on VPN settings reflects BEST-EFFORT mode', () => {
        const mojom = chromeos.networkConfig.mojom;
        mojoApi_.setAlwaysOnVpn({
          mode: mojom.AlwaysOnVpnMode.kBestEffort,
          serviceGuid: 'vpn1_guid',
        });
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.$$('#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              assertEquals(
                  mojom.AlwaysOnVpnMode.kBestEffort, networkAlwaysOnVpn.mode);
              assertEquals('vpn1_guid', networkAlwaysOnVpn.service);
            });
      });

      test('Always-on VPN settings reflects STRICT mode', () => {
        const mojom = chromeos.networkConfig.mojom;
        mojoApi_.setAlwaysOnVpn({
          mode: mojom.AlwaysOnVpnMode.kStrict,
          serviceGuid: 'vpn2_guid',
        });
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.$$('#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              assertEquals(
                  mojom.AlwaysOnVpnMode.kStrict, networkAlwaysOnVpn.mode);
              assertEquals('vpn2_guid', networkAlwaysOnVpn.service);
            });
      });

      test('Enabled always-on and select a service', () => {
        const mojom = chromeos.networkConfig.mojom;
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.$$('#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              networkAlwaysOnVpn.mode = mojom.AlwaysOnVpnMode.kBestEffort;
              networkAlwaysOnVpn.service = 'vpn1_guid';
              return flushAsync();
            })
            .then(() => mojoApi_.getAlwaysOnVpn())
            .then(result => {
              assertEquals(
                  mojom.AlwaysOnVpnMode.kBestEffort, result.properties.mode);
              assertEquals('vpn1_guid', result.properties.serviceGuid);
            });
      });

      test('Enable always-on with STRICT mode and select a service', () => {
        const mojom = chromeos.networkConfig.mojom;
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.$$('#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              networkAlwaysOnVpn.mode = mojom.AlwaysOnVpnMode.kStrict;
              networkAlwaysOnVpn.service = 'vpn2_guid';
              return flushAsync();
            })
            .then(() => mojoApi_.getAlwaysOnVpn())
            .then(result => {
              assertEquals(
                  mojom.AlwaysOnVpnMode.kStrict, result.properties.mode);
              assertEquals('vpn2_guid', result.properties.serviceGuid);
            });
      });

      test('Always-on VPN is not shown without networks', () => {
        return initSubpage().then(() => {
          const networkAlwaysOnVpn = internetSubpage.$$('#alwaysOnVpnSelector');
          assert(!networkAlwaysOnVpn);
        });
      });

      test('Always-on VPN list contains compatible networks', () => {
        const mojom = chromeos.networkConfig.mojom;
        mojoApi_.setAlwaysOnVpn({
          mode: mojom.AlwaysOnVpnMode.kBestEffort,
          serviceGuid: '',
        });
        return initSubpage()
            .then(() => {
              initVpn();
              return flushAsync();
            })
            .then(() => {
              const networkAlwaysOnVpn =
                  internetSubpage.$$('#alwaysOnVpnSelector');
              assert(networkAlwaysOnVpn);
              // The list should contain 2 compatible networks.
              assertEquals(2, networkAlwaysOnVpn.networks.length);
            });
      });
    });
  });
});
