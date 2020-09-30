// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// clang-format on

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
      settings.Router.getInstance().resetRouteForTesting();
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

    // Tests that when the route changes to one containing a deep link to
    // the shared proxy toggle, toggle is foxused.
    test('Deep link to shared proxy toggle', async () => {
      loadTimeData.overrideValues({isDeepLinkingEnabled: true});
      assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));

      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          mojom.NetworkType.kWiFi, 'wifi_device', mojom.OncSource.kDevice);
      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      const params = new URLSearchParams;
      params.append('guid', 'wifi_device_guid');
      params.append('type', 'WiFi');
      params.append('name', 'wifi_device');
      params.append('settingId', '11');
      settings.Router.getInstance().navigateTo(
          settings.routes.NETWORK_DETAIL, params);

      await flushAsync();

      const deepLinkElement = internetDetailPage.$$('network-proxy-section')
                                  .$$('#allowShared')
                                  .$$('#control');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Allow shared proxy toggle should be focused for settingId=11.');
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
        const disconnectButton = getButton('connectDisconnect');
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
        const disconnectButton = getButton('connectDisconnect');
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
        const connectButton = getButton('connectDisconnect');
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

    test('Cellular roaming subtext', async function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, true);
      mojoApi_.setManagedPropertiesForTest(
          getManagedProperties(mojom.NetworkType.kCellular, 'cellular'));
      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      await flushAsync();
      const roamingToggle = internetDetailPage.$$('#allowDataRoaming');
      assertTrue(!!roamingToggle);
      assertEquals(
          internetDetailPage.i18n('networkAllowDataRoamingDisabled'),
          roamingToggle.subLabel);

      const cellularNetwork =
          getManagedProperties(mojom.NetworkType.kCellular, 'cellular');
      cellularNetwork.typeProperties.cellular.allowRoaming = true;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);
      // Notify device state list change since allow roaming is
      // device level property in shill.
      internetDetailPage.onDeviceStateListChanged();
      await flushAsync();
      assertEquals(
          internetDetailPage.i18n('networkAllowDataRoamingEnabledHome'),
          roamingToggle.subLabel);
    });

    test('Deep link to disconnect button', async () => {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(mojom.NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      const params = new URLSearchParams;
      params.append('guid', 'cellular_guid');
      params.append('type', 'Cellular');
      params.append('name', 'cellular');
      params.append('settingId', '17');
      settings.Router.getInstance().navigateTo(
          settings.routes.NETWORK_DETAIL, params);

      await flushAsync();

      const deepLinkElement = getButton('connectDisconnect').$$('cr-button');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Disconnect network button should be focused for settingId=17.');
    });

    test('Deep link to sim lock toggle', async () => {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setDeviceStateForTest({
        type: mojom.NetworkType.kCellular,
        deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
        simLockStatus: {
          lockEnabled: false,
        },
      });
      const cellularNetwork =
          getManagedProperties(mojom.NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      const params = new URLSearchParams;
      params.append('guid', 'cellular_guid');
      params.append('type', 'Cellular');
      params.append('name', 'cellular');
      params.append('settingId', '14');
      settings.Router.getInstance().navigateTo(
          settings.routes.NETWORK_DETAIL, params);

      Polymer.dom.flush();

      const deepLinkElement =
          internetDetailPage.$$('network-siminfo').$$('#simLockButton');

      // In this rare case, wait after next render twice due to focus behavior
      // of the siminfo component.
      await test_util.waitAfterNextRender(deepLinkElement);
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Sim lock toggle should be focused for settingId=14.');
    });
  });

  suite('DetailsPageEthernet', function() {
    test('LoadPage', function() {});

    test('Eth1', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kEthernet, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kEthernet, 'eth1'),
      ]);

      internetDetailPage.init('eth1_guid', 'Ethernet', 'eth1');
      assertEquals('eth1_guid', internetDetailPage.guid);
      return flushAsync().then(() => {
        return mojoApi_.whenCalled('getManagedProperties');
      });
    });

    test('Deep link to configure ethernet button', async () => {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kEthernet, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kEthernet, 'eth1'),
      ]);

      const params = new URLSearchParams;
      params.append('guid', 'eth1_guid');
      params.append('type', 'Ethernet');
      params.append('name', 'eth1');
      params.append('settingId', '0');
      settings.Router.getInstance().navigateTo(
          settings.routes.NETWORK_DETAIL, params);

      await flushAsync();

      const deepLinkElement = getButton('configureButton');
      await test_util.waitAfterNextRender(deepLinkElement);

      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Configure ethernet button should be focused for settingId=0.');
    });
  });

  suite('DetailsPageTether', function() {
    test('LoadPage', function() {});

    test('Tether1', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kTether, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
      ]);

      internetDetailPage.init('tether1_guid', 'Tether', 'tether1');
      assertEquals('tether1_guid', internetDetailPage.guid);
      return flushAsync().then(() => {
        return mojoApi_.whenCalled('getManagedProperties');
      });
    });

    test('Deep link to disconnect tether network', async () => {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kTether, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kTether, 'tether1'),
      ]);
      const tetherNetwork =
          getManagedProperties(mojom.NetworkType.kTether, 'tether1');
      tetherNetwork.connectable = true;
      mojoApi_.setManagedPropertiesForTest(tetherNetwork);

      await flushAsync();

      const params = new URLSearchParams;
      params.append('guid', 'tether1_guid');
      params.append('type', 'Tether');
      params.append('name', 'tether1');
      params.append('settingId', '23');
      settings.Router.getInstance().navigateTo(
          settings.routes.NETWORK_DETAIL, params);

      await flushAsync();

      const deepLinkElement = getButton('connectDisconnect').$$('cr-button');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Disconnect tether button should be focused for settingId=23.');
    });
  });

  suite('DetailsPageAutoConnect', function() {
    test('Auto Connect toggle updates after GUID change', function() {
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

    test('Auto Connect updates don\'t trigger a re-save', function() {
      const mojom = chromeos.networkConfig.mojom;
      mojoApi_.setNetworkTypeEnabledState(mojom.NetworkType.kWiFi, true);
      let wifi1 = getManagedProperties(
          mojom.NetworkType.kWiFi, 'wifi1', mojom.OncSource.kDevice);
      wifi1.typeProperties.wifi.autoConnect = OncMojo.createManagedBool(true);
      mojoApi_.setManagedPropertiesForTest(wifi1);

      mojoApi_.whenCalled('setProperties').then(() => assert(false));
      internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi1');
      internetDetailPage.onNetworkStateChanged({guid: 'wifi1_guid'});
      return flushAsync()
          .then(() => {
            const toggle = internetDetailPage.$$('#autoConnectToggle');
            assertTrue(!!toggle);
            assertTrue(toggle.checked);

            // Rebuild the object to force polymer to recognize a change.
            wifi1 = getManagedProperties(
                mojom.NetworkType.kWiFi, 'wifi1', mojom.OncSource.kDevice);
            wifi1.typeProperties.wifi.autoConnect =
                OncMojo.createManagedBool(false);
            mojoApi_.setManagedPropertiesForTest(wifi1);
            internetDetailPage.onNetworkStateChanged({guid: 'wifi1_guid'});
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
