// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('InternetDetailPage', function() {
  /** @type {InternetDetailPageElement} */
  let internetDetailPage = null;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  /** @type {Object} */
  const prefs_ = {
    'vpn_config_allowed': {
      key: 'vpn_config_allowed',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    // Added use_shared_proxies because triggering a change in prefs_ without
    // it will fail a "Pref is missing" assertion in the network-proxy-section
    'settings': {
      'use_shared_proxies': {
        key: 'use_shared_proxies',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    },
  };

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

  function setNetworksForTest(networks) {
    mojoApi_.resetForTest();
    mojoApi_.addNetworksForTest(networks);
  }

  function getAllowSharedProxy() {
    const proxySection = internetDetailPage.$$('network-proxy-section');
    assertTrue(!!proxySection);
    const allowShared = proxySection.$$('#allowShared');
    assertTrue(!!allowShared);
    return allowShared;
  }

  function getButton(buttonId) {
    const button = internetDetailPage.$$(`#${buttonId}`);
    assertTrue(!!button);
    return button;
  }

  function getManagedProperties(type, name, opt_source) {
    const result =
        OncMojo.getDefaultManagedProperties(type, name + '_guid', name);
    if (opt_source) {
      result.source = opt_source;
    }
    return result;
  }

  setup(function() {
    PolymerTest.clearBody();
    internetDetailPage =
        document.createElement('settings-internet-detail-page');
    assertTrue(!!internetDetailPage);
    mojoApi_.resetForTest();
    internetDetailPage.prefs = Object.assign({}, prefs_);
    document.body.appendChild(internetDetailPage);
    return flushAsync();
  });

  teardown(function() {
    return flushAsync().then(() => {
      internetDetailPage.close();
      internetDetailPage.remove();
      internetDetailPage = null;
      settings.resetRouteForTesting();
    });
  });

  suite('DetailsPageWiFi', function() {
    test('LoadPage', function() {});

    test('WiFi1', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1'),
      ]);

      internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi1');
      assertEquals('wifi1_guid', internetDetailPage.guid);
      return flushAsync().then(() => {
        return mojoApi_.whenCalled('getManagedProperties');
      });
    });

    // Sanity test for the suite setup. Makes sure that re-opening the details
    // page with a different network also succeeds.
    test('WiFi2', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi2'),
      ]);

      internetDetailPage.init('wifi2_guid', 'WiFi', 'wifi2');
      assertEquals('wifi2_guid', internetDetailPage.guid);
      return flushAsync().then(() => {
        return mojoApi_.whenCalled('getManagedProperties');
      });
    });

    test('Proxy Unshared', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      const wifiNetwork =
          getManagedProperties(mojom.NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = mojom.OncSource.kUser;
      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const proxySection = internetDetailPage.$$('network-proxy-section');
        assertTrue(!!proxySection);
        const allowShared = proxySection.$$('#allowShared');
        assertTrue(!!allowShared);
        assertTrue(allowShared.hasAttribute('hidden'));
      });
    });

    test('Proxy Shared', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          mojom.NetworkType.kWiFi, 'wifi_device', mojom.OncSource.kDevice);
      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_device_guid', 'WiFi', 'wifi_device');
      return flushAsync().then(() => {
        const allowShared = getAllowSharedProxy();
        assertFalse(allowShared.hasAttribute('hidden'));
        assertFalse(allowShared.disabled);
      });
    });

    // When proxy settings are managed by a user policy but the configuration
    // is from the shared (device) profile, they still respect the
    // allowed_shared_proxies pref so #allowShared should be visible.
    // TOD(stevenjb): Improve this: crbug.com/662529.
    test('Proxy Shared User Managed', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          mojom.NetworkType.kWiFi, 'wifi_device', mojom.OncSource.kDevice);
      wifiNetwork.proxySetings = {
        type: {
          activeValue: 'Manual',
          policySource: mojom.PolicySource.kUserPolicyEnforced
        }
      };
      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_device_guid', 'WiFi', 'wifi_device');
      return flushAsync().then(() => {
        const allowShared = getAllowSharedProxy();
        assertFalse(allowShared.hasAttribute('hidden'));
        assertFalse(allowShared.disabled);
      });
    });

    // When proxy settings are managed by a device policy they may respect the
    // allowd_shared_proxies pref so #allowShared should be visible.
    test('Proxy Shared Device Managed', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          mojom.NetworkType.kWiFi, 'wifi_device', mojom.OncSource.kDevice);
      wifiNetwork.proxySetings = {
        type: {
          activeValue: 'Manual',
          policySource: mojom.PolicySource.kDevicePolicyEnforced
        }
      };
      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_device_guid', 'WiFi', 'wifi_device');
      return flushAsync().then(() => {
        const allowShared = getAllowSharedProxy();
        assertFalse(allowShared.hasAttribute('hidden'));
        assertFalse(allowShared.disabled);
      });
    });
  });

  suite('DetailsPageVPN', function() {
    test('VPN config allowed', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kVPN, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kVPN, 'vpn1'),
      ]);

      internetDetailPage.init('vpn1_guid', 'VPN', 'vpn1');
      prefs_.vpn_config_allowed.value = true;
      internetDetailPage.prefs = Object.assign({}, prefs_);
      return flushAsync().then(() => {
        const disconnectButton = getButton('disconnect');
        assertFalse(disconnectButton.hasAttribute('enforced_'));
        assertFalse(!!disconnectButton.$$('cr-policy-pref-indicator'));
      });
    });

    test('VPN config disallowed', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kVPN, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kVPN, 'vpn1'),
      ]);

      internetDetailPage.init('vpn1_guid', 'VPN', 'vpn1');
      prefs_.vpn_config_allowed.value = false;
      internetDetailPage.prefs = Object.assign({}, prefs_);
      return flushAsync().then(() => {
        const disconnectButton = getButton('disconnect');
        assertTrue(disconnectButton.hasAttribute('enforced_'));
        assertTrue(!!disconnectButton.$$('cr-policy-pref-indicator'));
      });
    });
  });

  suite('DetailsPageCellular', function() {
    test('Connect button disabled when not connectable', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(mojom.NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      return flushAsync().then(() => {
        const connectButton = getButton('connect');
        assertFalse(connectButton.hasAttribute('hidden'));
        assertTrue(connectButton.hasAttribute('disabled'));
      });
    });

    test('Cellular Scanning', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(mojom.NetworkType.kCellular, 'cellular');
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      mojoApi_.setDeviceStateForTest({
        type: mojom.NetworkType.kCellular,
        deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
        scanning: true,
      });

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      return flushAsync().then(() => {
        const spinner = internetDetailPage.$$('paper-spinner-lite');
        assertTrue(!!spinner);
        assertFalse(spinner.hasAttribute('hidden'));
      });
    });
  });

  suite('DetailsPageAutoConnect', function() {
    test.only('Auto Connect toggle updates after GUID change', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      const wifi1 = getManagedProperties(
          mojom.NetworkType.kWiFi, 'wifi1', mojom.OncSource.kDevice);
      wifi1.typeProperties.wifi.autoConnect = OncMojo.createManagedBool(true);
      mojoApi_.setManagedPropertiesForTest(wifi1);

      const wifi2 = getManagedProperties(
          mojom.NetworkType.kWiFi, 'wifi2', mojom.OncSource.kDevice);
      wifi2.typeProperties.wifi.autoConnect = OncMojo.createManagedBool(false);
      mojoApi_.setManagedPropertiesForTest(wifi2);

      internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi1');
      return flushAsync()
          .then(() => {
            const toggle = internetDetailPage.$$('#autoConnectToggle');
            assertTrue(!!toggle);
            assertTrue(toggle.checked);
            internetDetailPage.init('wifi2_guid', 'WiFi', 'wifi2');
            return flushAsync();
          })
          .then(() => {
            const toggle = internetDetailPage.$$('#autoConnectToggle');
            assertTrue(!!toggle);
            assertFalse(toggle.checked);
          });
    });
  });
});
