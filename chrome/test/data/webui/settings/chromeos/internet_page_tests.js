// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('InternetPage', function() {
  /** @type {?InternetPageElement} */
  let internetPage = null;

  /** @type {?NetworkSummaryElement} */
  let networkSummary_ = null;

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

  function setNetworksForTest(networks) {
    mojoApi_.resetForTest();
    mojoApi_.addNetworksForTest(networks);
  }

  setup(function() {
    PolymerTest.clearBody();
    internetPage = document.createElement('settings-internet-page');
    assertTrue(!!internetPage);
    mojoApi_.resetForTest();
    document.body.appendChild(internetPage);
    networkSummary_ = internetPage.$$('network-summary');
    assertTrue(!!networkSummary_);
    return flushAsync().then(() => {
      return Promise.all([
        mojoApi_.whenCalled('getNetworkStateList'),
        mojoApi_.whenCalled('getDeviceStateList'),
      ]);
    });
  });

  teardown(function() {
    const subPage = internetPage.$$('settings-internet-subpage');
    if (subPage) {
      subPage.remove();
    }
    const detailPage = internetPage.$$('settings-internet-detail-page');
    if (detailPage) {
      detailPage.remove();
    }
    internetPage.remove();
    internetPage = null;
    settings.resetRouteForTesting();
  });

  suite('MainPage', function() {
    test('Ethernet', function() {
      // Default fake device state is Ethernet enabled only.
      const ethernet = networkSummary_.$$('#Ethernet');
      assertTrue(!!ethernet);
      assertEquals(1, ethernet.networkStateList.length);
      assertEquals(null, networkSummary_.$$('#Cellular'));
      assertEquals(null, networkSummary_.$$('#VPN'));
      assertEquals(null, networkSummary_.$$('#WiFi'));
    });

    test('WiFi', function() {
      const mojom = chromeos.networkConfig.mojom;
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1'),
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi2'),
      ]);
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      return flushAsync().then(() => {
        const wifi = networkSummary_.$$('#WiFi');
        assertTrue(!!wifi);
        assertEquals(2, wifi.networkStateList.length);
      });
    });

    test('WiFiToggle', function() {
      const mojom = chromeos.networkConfig.mojom;
      // Make WiFi an available but disabled technology.
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, false);
      return flushAsync().then(() => {
        const wifi = networkSummary_.$$('#WiFi');
        assertTrue(!!wifi);

        // Ensure that the initial state is disabled and the toggle is
        // enabled but unchecked.
        const wifiDevice =
            mojoApi_.getDeviceStateForTest(mojom.NetworkType.kWiFi);
        assertTrue(!!wifiDevice);
        assertEquals(mojom.DeviceStateType.kDisabled, wifiDevice.deviceState);
        const toggle = wifi.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        assertFalse(toggle.checked);

        // Tap the enable toggle button and ensure the state becomes enabled.
        toggle.click();
        return flushAsync().then(() => {
          assertTrue(toggle.checked);
          const wifiDevice =
              mojoApi_.getDeviceStateForTest(mojom.NetworkType.kWiFi);
          assertTrue(!!wifiDevice);
          assertEquals(mojom.DeviceStateType.kEnabled, wifiDevice.deviceState);
        });
      });
    });

    test('VpnProviders', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setVpnProvidersForTest([
        {
          type: mojom.VpnType.kExtension,
          providerId: 'extension_id1',
          providerName: 'MyExtensionVPN1',
          appId: 'extension_id1',
          lastLaunchTime: {internalValue: 0},
        },
        {
          type: mojom.VpnType.kArc,
          providerId: 'vpn.app.package1',
          providerName: 'MyArcVPN1',
          appId: 'arcid1',
          lastLaunchTime: {internalValue: 1},
        },
        {
          type: mojom.VpnType.kArc,
          providerId: 'vpn.app.package2',
          providerName: 'MyArcVPN2',
          appId: 'arcid2',
          lastLaunchTime: {internalValue: 2},
        }
      ]);
      return flushAsync().then(() => {
        assertEquals(3, internetPage.vpnProviders_.length);
        // Ensure providers are sorted by type and lastLaunchTime.
        assertEquals('extension_id1', internetPage.vpnProviders_[0].providerId);
        assertEquals(
            'vpn.app.package2', internetPage.vpnProviders_[1].providerId);
        assertEquals(
            'vpn.app.package1', internetPage.vpnProviders_[2].providerId);
      });
    });
  });

  // TODO(stevenjb): Figure out a way to reliably test navigation. Currently
  // such tests are flaky.
});
