// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('InternetSubpage', function() {
  /** @type {?SettingsInternetSubpageElement} */
  let internetSubpage = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

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

    CrOncStrings = {
      OncTypeCellular: 'OncTypeCellular',
      OncTypeEthernet: 'OncTypeEthernet',
      OncTypeMobile: 'OncTypeMobile',
      OncTypeTether: 'OncTypeTether',
      OncTypeVPN: 'OncTypeVPN',
      OncTypeWiFi: 'OncTypeWiFi',
      networkListItemConnected: 'networkListItemConnected',
      networkListItemConnecting: 'networkListItemConnecting',
      networkListItemConnectingTo: 'networkListItemConnectingTo',
      networkListItemLabelTemplate: '%1 - %2',
      networkListItemNotConnected: 'networkListItemNotConnected',
      networkListItemNoNetwork: 'networkListItemNoNetwork',
      vpnNameTemplate: 'vpnNameTemplate',
    };

    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  function flushAsync() {
    Polymer.dom.flush();
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

  setup(function() {
    PolymerTest.clearBody();
    internetSubpage = document.createElement('settings-internet-subpage');
    assertTrue(!!internetSubpage);
    mojoApi_.resetForTest();
    document.body.appendChild(internetSubpage);
    internetSubpage.init();
    return flushAsync();
  });

  teardown(function() {
    internetSubpage.remove();
    internetSubpage = null;
    settings.resetRouteForTesting();
  });

  suite('SubPage', function() {
    test('WiFi', function() {
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

    test('Tether', function() {
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

    test('Tether plus Cellular', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kTether);
      setNetworksForTest(mojom.NetworkType.kCellular, [
        OncMojo.getDefaultNetworkState(
            mojom.NetworkType.kCellular, 'cellular1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether2'),
      ]);
      internetSubpage.tetherDeviceState = {
        type: mojom.NetworkType.kTether,
        deviceState: mojom.DeviceStateType.kEnabled
      };
      return flushAsync().then(() => {
        assertEquals(3, internetSubpage.networkStateList_.length);
        const toggle = internetSubpage.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = internetSubpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(3, networkList.networks.length);
        const tetherToggle = internetSubpage.$$('#tetherEnabledButton');
        assertTrue(!!tetherToggle);
        assertFalse(tetherToggle.disabled);
      });
    });

    test('VPN', function() {
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
      return flushAsync().then(() => {
        assertEquals(2, internetSubpage.networkStateList_.length);
        const allNetworkLists =
            internetSubpage.shadowRoot.querySelectorAll('network-list');
        // Internal networks + 2 extension ids + 1 arc id (package name) = 4
        assertEquals(4, allNetworkLists.length);
        // 2 internal networks
        assertEquals(2, allNetworkLists[0].networks.length);
        // 2 networks with extension id 'extension_id1'
        assertEquals(2, allNetworkLists[1].networks.length);
        // 1 network with extension id 'extension_id2'
        assertEquals(1, allNetworkLists[2].networks.length);
        // 1 connected network with arc id 'vpn.app.package1'
        assertEquals(1, allNetworkLists[3].networks.length);
      });
    });
  });
});
