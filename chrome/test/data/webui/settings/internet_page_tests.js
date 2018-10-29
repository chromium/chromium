// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('InternetPage', function() {
  /** @type {InternetPageElement} */
  let internetPage = null;

  /** @type {NetworkSummaryElement} */
  let networkSummary_ = null;

  /** @type {NetworkingPrivate} */
  let api_;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      internetAddConnection: 'internetAddConnection',
      internetAddConnectionExpandA11yLabel:
          'internetAddConnectionExpandA11yLabel',
      internetAddConnectionNotAllowed: 'internetAddConnectionNotAllowed',
      internetAddThirdPartyVPN: 'internetAddThirdPartyVPN',
      internetAddVPN: 'internetAddVPN',
      internetAddArcVPN: 'internetAddArcVPN',
      internetAddArcVPNProvider: 'internetAddArcVPNProvider',
      internetAddWiFi: 'internetAddWiFi',
      internetDetailPageTitle: 'internetDetailPageTitle',
      internetKnownNetworksPageTitle: 'internetKnownNetworksPageTitle',
    });

    CrOncStrings = {
      OncTypeCellular: 'OncTypeCellular',
      OncTypeEthernet: 'OncTypeEthernet',
      OncTypeTether: 'OncTypeTether',
      OncTypeVPN: 'OncTypeVPN',
      OncTypeWiFi: 'OncTypeWiFi',
      OncTypeWiMAX: 'OncTypeWiMAX',
      networkListItemConnected: 'networkListItemConnected',
      networkListItemConnecting: 'networkListItemConnecting',
      networkListItemConnectingTo: 'networkListItemConnectingTo',
      networkListItemNotConnected: 'networkListItemNotConnected',
      networkListItemNoNetwork: 'networkListItemNoNetwork',
      vpnNameTemplate: 'vpnNameTemplate',
    };

    api_ = new chrome.FakeNetworkingPrivate();

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      internetPage.async(resolve);
    });
  }

  function setNetworksForTest(networks) {
    api_.resetForTest();
    api_.addNetworksForTest(networks);
  }

  function setArcVpnProvidersForTest(arcVpnProviders) {
    cr.webUIListenerCallback('sendArcVpnProviders', arcVpnProviders);
  }

  setup(function() {
    PolymerTest.clearBody();
    internetPage = document.createElement('settings-internet-page');
    assertTrue(!!internetPage);
    api_.resetForTest();
    internetPage.networkingPrivate = api_;
    document.body.appendChild(internetPage);
    networkSummary_ = internetPage.$$('network-summary');
    assertTrue(!!networkSummary_);
    return flushAsync().then(() => {
      return Promise.all([
        api_.whenCalled('getNetworks'),
        api_.whenCalled('getDeviceStates'),
      ]);
    });
  });

  teardown(function() {
    internetPage.remove();
    delete internetPage;
    // Navigating to the details page changes the Route state.
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
      assertEquals(null, networkSummary_.$$('#WiMAX'));
      assertEquals(null, networkSummary_.$$('#WiFi'));
    });

    test('WiFi', function() {
      setNetworksForTest([
        {GUID: 'wifi1_guid', Name: 'wifi1', Type: 'WiFi'},
        {GUID: 'wifi12_guid', Name: 'wifi2', Type: 'WiFi'},
      ]);
      api_.enableNetworkType('WiFi');
      Polymer.dom.flush();
      const wifi = networkSummary_.$$('#WiFi');
      assertTrue(!!wifi);
      assertEquals(2, wifi.networkStateList.length);
    });

    test('WiFiToggle', function() {
      // Make WiFi an available but disabled technology.
      api_.disableNetworkType('WiFi');
      Polymer.dom.flush();
      const wifi = networkSummary_.$$('#WiFi');
      assertTrue(!!wifi);

      // Ensure that the initial state is disabled and the toggle is
      // enabled but unchecked.
      assertEquals('Disabled', api_.getDeviceStateForTest('WiFi').State);
      const toggle = wifi.$$('#deviceEnabledButton');
      assertTrue(!!toggle);
      assertFalse(toggle.disabled);
      assertFalse(toggle.checked);

      // Tap the enable toggle button and ensure the state becomes enabled.
      toggle.click();
      Polymer.dom.flush();
      assertTrue(toggle.checked);
      assertEquals('Enabled', api_.getDeviceStateForTest('WiFi').State);
    });
  });

  suite('SubPage', function() {
    test('WiFi', function() {
      setNetworksForTest([
        {GUID: 'wifi1_guid', Name: 'wifi1', Type: 'WiFi'},
        {GUID: 'wifi12_guid', Name: 'wifi2', Type: 'WiFi'},
      ]);
      api_.enableNetworkType('WiFi');
      Polymer.dom.flush();
      const wifi = networkSummary_.$$('#WiFi');
      assertTrue(!!wifi);
      wifi.$$('.subpage-arrow button').click();
      return flushAsync().then(() => {
        const subpage = internetPage.$$('settings-internet-subpage');
        assertTrue(!!subpage);
        assertEquals(2, subpage.networkStateList_.length);
        const toggle = wifi.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = subpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
      });
    });

    test('Cellular', function() {
      setNetworksForTest([
        {GUID: 'cellular1_guid', Name: 'cellular1', Type: 'Cellular'},
      ]);
      api_.enableNetworkType('Cellular');
      return flushAsync()
          .then(() => {
            return Promise.all([
              api_.whenCalled('getNetworks'),
              api_.whenCalled('getDeviceStates'),
            ]);
          })
          .then(() => {
            const mobile = networkSummary_.$$('#Cellular');
            assertTrue(!!mobile);
            mobile.$$('.subpage-arrow button').click();
            return Promise.all([
              api_.whenCalled('getManagedProperties'),
            ]);
          })
          .then(() => {
            const detailPage = internetPage.$$('settings-internet-detail-page');
            assertTrue(!!detailPage);
          });
    });

    test('Tether', function() {
      setNetworksForTest([
        {GUID: 'tether1_guid', Name: 'tether1', Type: 'Tether'},
        {GUID: 'tether2_guid', Name: 'tether2', Type: 'Tether'},
      ]);
      api_.enableNetworkType('Tether');
      return flushAsync().then(() => {
        const mobile = networkSummary_.$$('#Tether');
        assertTrue(!!mobile);
        mobile.$$('.subpage-arrow button').click();
        Polymer.dom.flush();
        const subpage = internetPage.$$('settings-internet-subpage');
        assertTrue(!!subpage);
        assertEquals(2, subpage.networkStateList_.length);
        const toggle = mobile.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = subpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
        const tetherToggle = mobile.$$('#tetherEnabledButton');
        // No separate tether toggle when Celular is not available; the
        // primary toggle enables or disables Tether in that case.
        assertFalse(!!tetherToggle);
      });
    });

    test('Tether plus Cellular', function() {
      setNetworksForTest([
        {GUID: 'cellular1_guid', Name: 'cellular1', Type: 'Cellular'},
        {GUID: 'tether1_guid', Name: 'tether1', Type: 'Tether'},
        {GUID: 'tether2_guid', Name: 'tether2', Type: 'Tether'},
      ]);
      api_.enableNetworkType('Cellular');
      api_.enableNetworkType('Tether');
      return flushAsync().then(() => {
        const mobile = networkSummary_.$$('#Cellular');
        assertTrue(!!mobile);
        mobile.$$('.subpage-arrow button').click();
        Polymer.dom.flush();
        const subpage = internetPage.$$('settings-internet-subpage');
        assertTrue(!!subpage);
        assertEquals(3, subpage.networkStateList_.length);
        const toggle = mobile.$$('#deviceEnabledButton');
        assertTrue(!!toggle);
        assertFalse(toggle.disabled);
        const networkList = subpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(3, networkList.networks.length);
        const tetherToggle = subpage.$$('#tetherEnabledButton');
        assertTrue(!!tetherToggle);
        assertFalse(tetherToggle.disabled);
      });
    });

    test('VPN', function() {
      setNetworksForTest([
        {GUID: 'vpn1_guid', Name: 'vpn1', Type: 'VPN'},
        {GUID: 'vpn2_guid', Name: 'vpn1', Type: 'VPN'},
        {
          GUID: 'third_party1_vpn1_guid',
          Name: 'vpn3',
          Type: 'VPN',
          VPN: {
            Type: 'ThirdPartyVPN',
            ThirdPartyVPN: {ExtensionID: 'id1', ProviderName: 'pname1'}
          }
        },
        {
          GUID: 'third_party1_vpn2_guid',
          Name: 'vpn4',
          Type: 'VPN',
          VPN: {
            Type: 'ThirdPartyVPN',
            ThirdPartyVPN: {ExtensionID: 'id1', ProviderName: 'pname1'}
          }
        },
        {
          GUID: 'third_party2_vpn1_guid',
          Name: 'vpn5',
          Type: 'VPN',
          VPN: {
            Type: 'ThirdPartyVPN',
            ThirdPartyVPN: {ExtensionID: 'id2', ProviderName: 'pname2'}
          }
        },
      ]);
      api_.onNetworkListChanged.callListeners();
      return flushAsync().then(() => {
        const vpn = networkSummary_.$$('#VPN');
        assertTrue(!!vpn);
        vpn.$$('.subpage-arrow button').click();
        Polymer.dom.flush();
        const subpage = internetPage.$$('settings-internet-subpage');
        assertTrue(!!subpage);
        assertEquals(2, subpage.networkStateList_.length);
        const networkList = subpage.$$('#networkList');
        assertTrue(!!networkList);
        assertEquals(2, networkList.networks.length);
        // TODO(stevenjb): Implement fake management API and test third
        // party provider sections.
      });
    });

    test('ArcVPNProvider', function() {
      setArcVpnProvidersForTest([
        {
          Packagename: 'vpn.app.pacakge1',
          ProviderName: 'MyArcVPN1',
          AppID: 'arcid1',
          LastLaunchTime: 0
        },
        {
          Packagename: 'vpn.app.pacakge2',
          ProviderName: 'MyArcVPN2',
          AppID: 'arcid2',
          LastLaunchTime: 1
        }
      ]);
      return flushAsync().then(() => {
        const expandAddConnections = internetPage.$$('#expandAddConnections');
        assertTrue(!!expandAddConnections);
        assertTrue(!expandAddConnections.expanded);
        internetPage.addConnectionExpanded_ = true;
        Polymer.dom.flush();
        const addArcVpn = internetPage.$$('#addArcVpn');
        assertTrue(!!addArcVpn);
        addArcVpn.click();
        Polymer.dom.flush();
        const subpage = internetPage.$$('settings-internet-subpage');
        assertTrue(!!subpage);
        assertEquals(2, subpage.arcVpnProviders.length);
      });
    });

    test('WiFi Detail', function() {
      setNetworksForTest([
        {GUID: 'wifi1_guid', Name: 'wifi1', Type: 'WiFi'},
      ]);
      api_.enableNetworkType('WiFi');
      return flushAsync()
          .then(() => {
            const wifi = networkSummary_.$$('#WiFi');
            assertTrue(!!wifi);
            wifi.$$('.subpage-arrow button').click();
            return flushAsync();
          })
          .then(() => {
            // Call setTimeout to populate iron-list.
            return new Promise((resolve) => {
              setTimeout(function() {
                Polymer.dom.flush();
                resolve();
              });
            });
          })
          .then(() => {
            const subpage = internetPage.$$('settings-internet-subpage');
            assertTrue(!!subpage);
            const networkList = subpage.$$('#networkList');
            assertTrue(!!networkList);
            assertEquals(1, networkList.networks.length);
            assertEquals(1, networkList.listItems_.length);
            const ironList = networkList.$$('iron-list');
            assertTrue(!!ironList);
            assertEquals(1, ironList.items.length);
            const networkListItem = networkList.$$('cr-network-list-item');
            assertTrue(!!networkListItem);
            networkListItem.click();
            return flushAsync();
          })
          .then(() => {
            const detailPage = internetPage.$$('settings-internet-detail-page');
            assertTrue(!!detailPage);
            assertEquals('wifi1_guid', detailPage.guid);
            return Promise.all([
              api_.whenCalled('getManagedProperties'),
            ]);
          });
    });
  });
});
