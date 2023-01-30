// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InternetPageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {ActivationStateType, CrosNetworkConfigRemote, InhibitReason, ManagedProperties, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType, OncSource, PolicySource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestInternetPageBrowserProxy} from './test_internet_page_browser_proxy.js';

suite('InternetDetailPage', function() {
  /** @type {InternetDetailPageElement} */
  let internetDetailPage = null;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  /** @type {?TestInternetPageBrowserProxy} */
  let browserProxy = null;

  /** @type {Object} */
  const prefs_ = {
    'vpn_config_allowed': {
      key: 'vpn_config_allowed',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    'cros': {
      'signed': {
        'data_roaming_enabled': {
          key: 'data_roaming_enabled',
          value: true,
          controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
        },
      },
    },
    // Added use_shared_proxies and lacros_proxy_controlling_extension because
    // triggering a change in prefs_ without it will fail a "Pref is missing"
    // assertion in the network-proxy-section
    'settings': {
      'use_shared_proxies': {
        key: 'use_shared_proxies',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    },
    'ash': {
      'lacros_proxy_controlling_extension': {
        key: 'ash.lacros_proxy_controlling_extension',
        type: chrome.settingsPrivate.PrefType.DICTIONARY,
        value: {},
      },
    },
  };

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function setNetworksForTest(networks) {
    mojoApi_.resetForTest();
    mojoApi_.addNetworksForTest(networks);
  }

  function getAllowSharedProxy() {
    const proxySection =
        internetDetailPage.shadowRoot.querySelector('network-proxy-section');
    assertTrue(!!proxySection);
    const allowShared = proxySection.shadowRoot.querySelector('#allowShared');
    assertTrue(!!allowShared);
    return allowShared;
  }

  function getButton(buttonId) {
    const button = internetDetailPage.shadowRoot.querySelector(`#${buttonId}`);
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

  function getHiddenToggle() {
    if (loadTimeData.getBoolean('enableHiddenNetworkMigration')) {
      return internetDetailPage.shadowRoot.querySelector('#hiddenToggle');
    }
    return internetDetailPage.shadowRoot.querySelector('#hiddenToggleLegacy');
  }

  /**
   * @param {boolean} isSimLocked
   */
  function deepLinkToSimLockElement(isSimLocked) {
    init();

    const test_iccid = '11111111111111111';
    mojoApi_.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simLockStatus: {
        lockEnabled: true,
        lockType: isSimLocked ? 'sim-pin' : undefined,
      },
      simInfos: [{
        iccid: test_iccid,
        isPrimary: true,
      }],
    });

    const cellularNetwork =
        getManagedProperties(NetworkType.kCellular, 'cellular');
    cellularNetwork.connectable = false;
    cellularNetwork.typeProperties.cellular.iccid = test_iccid;
    mojoApi_.setManagedPropertiesForTest(cellularNetwork);

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    params.append('type', 'Cellular');
    params.append('name', 'cellular');
    params.append('settingId', '14');
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    return flushAsync();
  }

  setup(function() {
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
      showMeteredToggle: true,
      isApnRevampEnabled: false,
    });

    PolymerTest.clearBody();
    mojoApi_.resetForTest();

    browserProxy = new TestInternetPageBrowserProxy();
    InternetPageBrowserProxyImpl.setInstance(browserProxy);

    return flushAsync();
  });

  teardown(function() {
    return flushAsync().then(() => {
      internetDetailPage.close();
      internetDetailPage.remove();
      internetDetailPage = null;
      Router.getInstance().resetRouteForTesting();
    });
  });

  /**
   * @param {boolean=} opt_doNotProvidePrefs If provided, determine whether
   *     prefs should be provided for the element.
   */
  function init(opt_doNotProvidePrefs) {
    internetDetailPage =
        document.createElement('settings-internet-detail-page');
    assertTrue(!!internetDetailPage);
    if (!opt_doNotProvidePrefs) {
      internetDetailPage.prefs = Object.assign({}, prefs_);
    }
    document.body.appendChild(internetDetailPage);
  }

  suite('DetailsPageWiFi', function() {
    test('LoadPage', function() {
      init();
    });

    test('WiFi1', function() {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1'),
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
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2'),
      ]);

      internetDetailPage.init('wifi2_guid', 'WiFi', 'wifi2');
      assertEquals('wifi2_guid', internetDetailPage.guid);
      return flushAsync().then(() => {
        return mojoApi_.whenCalled('getManagedProperties');
      });
    });

    test('WiFi in a portal portalState', function() {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kPortal;

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.isCaptivePortalUI2022Enabled_ = true;
      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const networkStateText =
            internetDetailPage.shadowRoot.querySelector(`#networkState`);
        assertTrue(networkStateText.hasAttribute('warning'));
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailPage.i18n('networkListItemSignIn'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertFalse(signinButton.hasAttribute('hidden'));
        assertFalse(signinButton.disabled);
      });
    });

    test('WiFi in a portal-suspected portalState', function() {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kPortalSuspected;

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.isCaptivePortalUI2022Enabled_ = true;
      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const networkStateText =
            internetDetailPage.shadowRoot.querySelector(`#networkState`);
        assertTrue(networkStateText.hasAttribute('warning'));
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailPage.i18n('networkListItemConnectedLimited'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertTrue(signinButton.hasAttribute('hidden'));
        assertTrue(signinButton.disabled);
      });
    });

    test('WiFi in a portal-suspected portalState', function() {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kNoInternet;

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.isCaptivePortalUI2022Enabled_ = true;
      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const networkStateText =
            internetDetailPage.shadowRoot.querySelector(`#networkState`);
        assertTrue(networkStateText.hasAttribute('warning'));
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailPage.i18n('networkListItemConnectedNoConnectivity'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertTrue(signinButton.hasAttribute('hidden'));
        assertTrue(signinButton.disabled);
      });
    });

    test('WiFi in a proxy-auth portalState', function() {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kProxyAuthRequired;

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.isCaptivePortalUI2022Enabled_ = true;
      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const networkStateText =
            internetDetailPage.shadowRoot.querySelector(`#networkState`);
        assertTrue(networkStateText.hasAttribute('warning'));
        assertEquals(
            networkStateText.textContent.trim(),
            internetDetailPage.i18n('networkListItemSignIn'));
        const signinButton = getButton('signinButton');
        assertTrue(!!signinButton);
        assertFalse(signinButton.hasAttribute('hidden'));
        assertFalse(signinButton.disabled);
      });
    });

    test(
        'WiFi in a portal portalState with the feature flag disabled',
        function() {
          init();
          mojoApi_.resetForTest();
          mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
          const wifiNetwork =
              getManagedProperties(NetworkType.kWiFi, 'wifi_user');
          wifiNetwork.source = OncSource.kUser;
          wifiNetwork.connectable = true;
          wifiNetwork.connectionState = ConnectionStateType.kPortal;
          wifiNetwork.portalState = PortalState.kProxyAuthRequired;

          mojoApi_.setManagedPropertiesForTest(wifiNetwork);

          internetDetailPage.isCaptivePortalUI2022Enabled_ = false;
          internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
          return flushAsync().then(() => {
            const networkStateText =
                internetDetailPage.shadowRoot.querySelector(`#networkState`);
            assertTrue(networkStateText.hasAttribute('connected'));
            assertEquals(
                networkStateText.textContent.trim(),
                internetDetailPage.i18n('OncConnected'));
            const signinButton =
                internetDetailPage.shadowRoot.querySelector(`#signinButton`);
            // Button does not exist because feature flag is disabled.
            assertTrue(!signinButton);
          });
        });

    test('Hidden toggle enabled', function() {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.typeProperties.wifi.hiddenSsid =
          OncMojo.createManagedBool(true);

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const hiddenToggle = getHiddenToggle();
        assertTrue(!!hiddenToggle);
        assertTrue(hiddenToggle.checked);
      });
    });

    test('Hidden toggle disabled', function() {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.typeProperties.wifi.hiddenSsid =
          OncMojo.createManagedBool(false);

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const hiddenToggle = getHiddenToggle();
        assertTrue(!!hiddenToggle);
        assertFalse(hiddenToggle.checked);
      });
    });

    test('Hidden toggle hidden when not configured', function() {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.connectable = false;
      wifiNetwork.typeProperties.wifi.hiddenSsid =
          OncMojo.createManagedBool(false);

      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const hiddenToggle = getHiddenToggle();
        assertFalse(!!hiddenToggle);
      });
    });

    // Syntactic sugar for running test twice with different values for the
    // apnRevamp feature flag.
    [true, false].forEach(shouldEnableHiddenNetworkMigration => {
      test(
          'Hidden toggle is shown in a different location depending on feature flag',
          async () => {
            loadTimeData.overrideValues({
              enableHiddenNetworkMigration: shouldEnableHiddenNetworkMigration,
            });

            init();
            mojoApi_.resetForTest();
            mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
            const wifiNetwork =
                getManagedProperties(NetworkType.kWiFi, 'wifi_user');
            wifiNetwork.source = OncSource.kUser;
            wifiNetwork.connectable = true;

            mojoApi_.setManagedPropertiesForTest(wifiNetwork);

            internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');

            return flushAsync().then(() => {
              const enableHiddenNetworkMigration =
                  loadTimeData.getBoolean('enableHiddenNetworkMigration');
              const hiddenToggle =
                  internetDetailPage.shadowRoot.querySelector('#hiddenToggle');
              const hiddenToggleLegacy =
                  internetDetailPage.shadowRoot.querySelector(
                      '#hiddenToggleLegacy');
              if (loadTimeData.getBoolean('enableHiddenNetworkMigration')) {
                assertTrue(!!hiddenToggle);
                assertFalse(!!hiddenToggleLegacy);
              } else {
                assertFalse(!!hiddenToggle);
                assertTrue(!!hiddenToggleLegacy);
              }
            });
          });
    });

    test('Proxy Unshared', function() {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      return flushAsync().then(() => {
        const proxySection = internetDetailPage.shadowRoot.querySelector(
            'network-proxy-section');
        assertTrue(!!proxySection);
        const allowShared =
            proxySection.shadowRoot.querySelector('#allowShared');
        assertTrue(!!allowShared);
        assertTrue(allowShared.hasAttribute('hidden'));
      });
    });

    test('Proxy Shared', function() {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          NetworkType.kWiFi, 'wifi_device', OncSource.kDevice);
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
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          NetworkType.kWiFi, 'wifi_device', OncSource.kDevice);
      wifiNetwork.proxySetings = {
        type: {
          activeValue: 'Manual',
          policySource: PolicySource.kUserPolicyEnforced,
        },
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
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          NetworkType.kWiFi, 'wifi_device', OncSource.kDevice);
      wifiNetwork.proxySetings = {
        type: {
          activeValue: 'Manual',
          policySource: PolicySource.kDevicePolicyEnforced,
        },
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
    // the shared proxy toggle, toggle is focused.
    test('Deep link to shared proxy toggle', async () => {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          NetworkType.kWiFi, 'wifi_device', OncSource.kDevice);
      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      const params = new URLSearchParams();
      params.append('guid', 'wifi_device_guid');
      params.append('type', 'WiFi');
      params.append('name', 'wifi_device');
      params.append('settingId', '11');
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await flushAsync();

      const deepLinkElement =
          internetDetailPage.shadowRoot.querySelector('network-proxy-section')
              .shadowRoot.querySelector('#allowShared')
              .shadowRoot.querySelector('#control');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Allow shared proxy toggle should be focused for settingId=11.');
    });

    test('WiFi page disabled when blocked by policy', async () => {
      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      mojoApi_.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      internetDetailPage.globalPolicy = {
        allowOnlyPolicyWifiNetworksToConnect: true,
      };
      await flushAsync();

      const connectDisconnectButton = getButton('connectDisconnect');
      assertTrue(connectDisconnectButton.hidden);
      assertTrue(connectDisconnectButton.disabled);
      assertFalse(!!internetDetailPage.shadowRoot.querySelector('#infoFields'));
      const configureButton = getButton('configureButton');
      assertTrue(configureButton.hidden);
      const advancedFields = getButton('advancedFields');
      assertFalse(advancedFields.disabled);
      assertFalse(advancedFields.hidden);
      assertFalse(
          !!internetDetailPage.shadowRoot.querySelector('#deviceFields'));
      assertFalse(
          !!internetDetailPage.shadowRoot.querySelector('network-ip-config'));
      assertFalse(
          !!internetDetailPage.shadowRoot.querySelector('network-nameservers'));
      assertFalse(!!internetDetailPage.shadowRoot.querySelector(
          'network-proxy-section'));
    });
  });

  suite('DetailsPageVPN', function() {
    /**
     * @param {boolean=} opt_doNotProvidePrefs If provided, determine whether
     *     prefs should be provided for the element.
     */
    function initVpn(opt_doNotProvidePrefs) {
      init(opt_doNotProvidePrefs);
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kVPN, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn1'),
      ]);

      internetDetailPage.init('vpn1_guid', 'VPN', 'vpn1');
    }

    /**
     * @param {ManagedProperties} managedProperties
     *     Managed properties used to initialize the network.
     */
    function initManagedVpn(managedProperties) {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kVPN, true);
      mojoApi_.resetForTest();
      mojoApi_.addNetworksForTest([
        OncMojo.managedPropertiesToNetworkState(managedProperties),
      ]);
      mojoApi_.setManagedPropertiesForTest(managedProperties);
      internetDetailPage.init(
          managedProperties.guid, 'VPN', managedProperties.name.activeValue);
    }

    /**
     * @param {chromeos.networConfig.OncSource=} opt_oncSource If
     *     provided, sets the source (user / device / policy) of the network.
     */
    function initAdvancedVpn(opt_oncSource) {
      const vpn1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'vpn1_guid', 'vpn1');
      vpn1.source = opt_oncSource;
      vpn1.typeProperties.vpn.type = VpnType.kOpenVPN;
      vpn1.typeProperties.vpn.openVpn = {
        auth: 'MD5',
        cipher: 'AES-192-CBC',
        compressionAlgorithm: 'LZO',
        tlsAuthContents: 'FAKE_CREDENTIAL_VPaJDV9x',
        keyDirection: '1',
      };
      initManagedVpn(vpn1);
    }

    function initVpnWithNoAdvancedProperties() {
      const vpn1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'vpn1_guid', 'vpn1');
      vpn1.source = OncSource.kUserPolicy;
      vpn1.typeProperties.vpn.type = VpnType.kOpenVPN;
      // Try out all the values considered "empty" to make sure we do not
      // register any of them as set.
      vpn1.typeProperties.vpn.openVpn = {
        auth: '',
        cipher: undefined,
        compressionAlgorithm: null,
      };
      initManagedVpn(vpn1);
    }

    function initWireGuard() {
      const wg1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'wg1_guid', 'wg1');
      wg1.typeProperties.vpn.type = VpnType.kWireGuard;
      wg1.typeProperties.vpn.wireguard = {
        peers: {
          activeValue: [{
            publicKey: 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=',
            endpoint: '192.168.66.66:32000',
            allowedIps: '0.0.0.0/0',
          }],
        },
      };
      wg1.staticIpConfig = {ipAddress: {activeValue: '10.10.0.1'}};
      initManagedVpn(wg1);
    }

    test('VPN config allowed', function() {
      initVpn();
      prefs_.vpn_config_allowed.value = true;
      internetDetailPage.prefs = Object.assign({}, prefs_);
      return flushAsync().then(() => {
        const disconnectButton = getButton('connectDisconnect');
        assertFalse(disconnectButton.hasAttribute('enforced_'));
        assertFalse(!!disconnectButton.shadowRoot.querySelector(
            'cr-policy-pref-indicator'));
      });
    });

    test('VPN config disallowed', function() {
      initVpn();
      prefs_.vpn_config_allowed.value = false;
      internetDetailPage.prefs = Object.assign({}, prefs_);
      return flushAsync().then(() => {
        const disconnectButton = getButton('connectDisconnect');
        assertTrue(disconnectButton.hasAttribute('enforced_'));
        assertTrue(!!disconnectButton.shadowRoot.querySelector(
            'cr-policy-pref-indicator'));
      });
    });

    test('Managed VPN with advanced fields', function() {
      initAdvancedVpn(OncSource.kUserPolicy);
      return flushAsync().then(() => {
        assertTrue(
            !!internetDetailPage.shadowRoot.querySelector('#advancedFields'));
      });
    });

    test('Unmanaged VPN with advanced fields', function() {
      initAdvancedVpn(OncSource.kUser);
      return flushAsync().then(() => {
        assertFalse(
            !!internetDetailPage.shadowRoot.querySelector('#advancedFields'));
      });
    });

    // Regression test for issue fixed as part of https://crbug.com/1191626
    // where page would throw an exception if prefs were undefined. Prefs are
    // expected to be undefined if InternetDetailPage is loaded directly (e.g.,
    // when the user clicks on the network in Quick Settings).
    test('VPN without prefs', function() {
      initVpn(/*opt_doNotProvidePrefs=*/ true);
      return flushAsync();
    });

    test('OpenVPN does not show public key field', function() {
      initVpn();
      return flushAsync().then(() => {
        assertFalse(
            !!internetDetailPage.shadowRoot.querySelector('#wgPublicKeyField'));
      });
    });

    test('WireGuard does show public key field', function() {
      initWireGuard();
      return flushAsync().then(() => {
        assertTrue(
            !!internetDetailPage.shadowRoot.querySelector('#wgPublicKeyField'));
      });
    });

    test('Advanced section hidden when properties are not set', function() {
      initVpnWithNoAdvancedProperties();
      return flushAsync().then(() => {
        const expandButtons = internetDetailPage.shadowRoot.querySelectorAll(
            'cr-expand-button.settings-box');
        expandButtons.forEach(button => {
          assertNotEquals('Advanced', button.textContent.trim());
        });
      });
    });

  });

  suite('DetailsPageCellular', function() {
    async function expandConfigurableSection() {
      const configurableSections =
          internetDetailPage.shadowRoot.querySelector('#configurableSections');
      assertTrue(!!configurableSections);
      configurableSections.click();
      await flushAsync();
      assertTrue(internetDetailPage.showConfigurableSections_);
    }
    // Regression test for https://crbug.com/1182884.
    test('Connect button enabled when not connectable', function() {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      return flushAsync().then(() => {
        const connectButton = getButton('connectDisconnect');
        assertFalse(connectButton.hasAttribute('hidden'));
        assertFalse(connectButton.hasAttribute('disabled'));
      });
    });

    test('Connect button disabled when not connectable and locked', function() {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      cellularNetwork.typeProperties.cellular.simLocked = true;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      return flushAsync().then(() => {
        const connectButton = getButton('connectDisconnect');
        assertFalse(connectButton.hasAttribute('hidden'));
        assertTrue(connectButton.hasAttribute('disabled'));
      });
    });

    test(
        'Cellular view account button opens carrier account details',
        function() {
          init();
          mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
          const cellularNetwork =
              getManagedProperties(NetworkType.kCellular, 'cellular');
          mojoApi_.setManagedPropertiesForTest(cellularNetwork);

          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          return flushAsync()
              .then(() => {
                const viewAccountButton =
                    internetDetailPage.shadowRoot.querySelector(
                        '#viewAccountButton');
                assertTrue(!!viewAccountButton);
                viewAccountButton.click();
                return flushAsync();
              })
              .then(() => {
                return browserProxy.whenCalled('showCarrierAccountDetail');
              });
        });

    test(
        'Unactivated eSIM does not show activate or view account button',
        function() {
          init();
          mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
          const cellularNetwork =
              getManagedProperties(NetworkType.kCellular, 'cellular');
          cellularNetwork.typeProperties.cellular.eid = 'eid';
          cellularNetwork.connectionState = ConnectionStateType.kConnected;
          cellularNetwork.typeProperties.cellular.activationState =
              ActivationStateType.kNotActivated;
          cellularNetwork.typeProperties.cellular.paymentPortal = {url: 'url'};
          mojoApi_.setManagedPropertiesForTest(cellularNetwork);

          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          return flushAsync().then(() => {
            assertTrue(
                internetDetailPage.shadowRoot.querySelector('#activateButton')
                    .hidden);
            assertTrue(internetDetailPage.shadowRoot
                           .querySelector('#viewAccountButton')
                           .hidden);
          });
        });

    test('Cellular Scanning', function() {
      const test_iccid = '11111111111111111';

      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.typeProperties.cellular.iccid = test_iccid;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      mojoApi_.setDeviceStateForTest({
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        scanning: true,
        inhibitReason: InhibitReason.kNotInhibited,
        simInfos: [{
          iccid: test_iccid,
          isPrimary: true,
        }],
      });

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      return flushAsync().then(() => {
        const spinner =
            internetDetailPage.shadowRoot.querySelector('paper-spinner-lite');
        assertTrue(!!spinner);
        assertFalse(spinner.hasAttribute('hidden'));
      });
    });

    // Regression test for https://crbug.com/1201449.
    test('Page closed while device is updating', function() {
      init();

      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      mojoApi_.setDeviceStateForTest({
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        scanning: true,
      });

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');

      return flushAsync().then(() => {
        // Close the page as soon as getDeviceStateList() is invoked, before the
        // callback returns.
        mojoApi_.beforeGetDeviceStateList = () => {
          internetDetailPage.close();
        };

        mojoApi_.onDeviceStateListChanged();

        return flushAsync();
      });
    });

    test('Deep link to disconnect button', async () => {
      // Add listener for popstate event fired when the dialog closes and the
      // router navigates backwards.
      const popStatePromise = eventToPromise('popstate', window);

      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      const params = new URLSearchParams();
      params.append('guid', 'cellular_guid');
      params.append('type', 'Cellular');
      params.append('name', 'cellular');
      params.append('settingId', '17');
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await flushAsync();

      const deepLinkElement =
          getButton('connectDisconnect').shadowRoot.querySelector('cr-button');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Disconnect network button should be focused for settingId=17.');

      // Close the dialog and wait for os_route's popstate listener to fire. If
      // we don't add this wait, this event can fire during the next test which
      // will interfere with its routing.
      internetDetailPage.close();
      await popStatePromise;
    });

    test('Deep link to cellular roaming toggle button', async () => {
      const test_iccid = '11111111111111111';

      init();
      mojoApi_.resetForTest();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.typeProperties.cellular.iccid = test_iccid;
      cellularNetwork.connectable = false;
      // Required for allowDataRoamingButton to be rendered.
      cellularNetwork.typeProperties.cellular.allowRoaming =
          OncMojo.createManagedBool(false);
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      // Set SIM as active so that configurable sections are displayed.
      mojoApi_.setDeviceStateForTest({
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        inhibitReason: InhibitReason.kNotInhibited,
        simInfos: [{
          iccid: test_iccid,
          isPrimary: true,
        }],
      });

      const params = new URLSearchParams();
      params.append('guid', 'cellular_guid');
      params.append('type', 'Cellular');
      params.append('name', 'cellular');
      params.append('settingId', '15');
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await flushAsync();

      // Attempting to focus a <network-config-toggle> will result in the focus
      // being pushed onto the internal <cr-toggle>.
      const cellularRoamingToggle =
          internetDetailPage.shadowRoot
              .querySelector('cellular-roaming-toggle-button')
              .getCellularRoamingToggle();
      const deepLinkElement =
          cellularRoamingToggle.shadowRoot.querySelector('cr-toggle');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Cellular roaming toggle button should be focused for settingId=15.');
    });

    test('Deep link to sim lock toggle', async () => {
      await deepLinkToSimLockElement(/*isSimLocked=*/ false);

      const simInfo = internetDetailPage.shadowRoot.querySelector(
          '#cellularSimInfoAdvanced');

      // In this rare case, wait after next render twice due to focus behavior
      // of the siminfo component.
      await waitAfterNextRender(simInfo);
      await waitAfterNextRender(simInfo);
      assertEquals(
          simInfo.shadowRoot.querySelector('#simLockButton'),
          getDeepActiveElement(),
          'Sim lock toggle should be focused for settingId=14.');
    });

    test('Deep link to sim unlock button', async () => {
      await deepLinkToSimLockElement(/*isSimLocked=*/ true);

      const simInfo = internetDetailPage.shadowRoot.querySelector(
          '#cellularSimInfoAdvanced');

      // In this rare case, wait after next render twice due to focus behavior
      // of the siminfo component.
      await waitAfterNextRender(simInfo);
      await waitAfterNextRender(simInfo);
      assertEquals(
          simInfo.shadowRoot.querySelector('#unlockPinButton'),
          getDeepActiveElement(),
          'Sim unlock button should be focused for settingId=14.');
    });

    test('Cellular page hides hidden toggle', function() {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      return flushAsync().then(() => {
        const hiddenToggle = getHiddenToggle();
        assertFalse(!!hiddenToggle);
      });
    });

    test(
        'Cellular network on active sim slot, show config sections',
        async () => {
          init();
          const test_iccid = '11111111111111111';

          await mojoApi_.setNetworkTypeEnabledState(
              NetworkType.kCellular, true);
          const cellularNetwork = getManagedProperties(
              NetworkType.kCellular, 'cellular', OncSource.kDevice);
          cellularNetwork.typeProperties.cellular.iccid = test_iccid;
          // Required for allowDataRoamingButton to be rendered.
          cellularNetwork.typeProperties.cellular.allowRoaming =
              OncMojo.createManagedBool(false);

          mojoApi_.setManagedPropertiesForTest(cellularNetwork);
          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          mojoApi_.setDeviceStateForTest({
            type: NetworkType.kCellular,
            deviceState: DeviceStateType.kEnabled,
            inhibitReason: InhibitReason.kNotInhibited,
            simInfos: [{
              iccid: test_iccid,
              isPrimary: true,
            }],
          });
          await flushAsync();
          assertTrue(internetDetailPage.showConfigurableSections_);
          // Check that an element from the primary account section exists.
          assertTrue(!!internetDetailPage.shadowRoot
                           .querySelector('cellular-roaming-toggle-button')
                           .getCellularRoamingToggle());
        });

    test(
        'Cellular network on non-active sim slot, hide config sections',
        async () => {
          init();
          const test_iccid = '11111111111111111';

          await mojoApi_.setNetworkTypeEnabledState(
              NetworkType.kCellular, true);
          const cellularNetwork = getManagedProperties(
              NetworkType.kCellular, 'cellular', OncSource.kDevice);
          cellularNetwork.typeProperties.cellular.iccid = '000';

          mojoApi_.setManagedPropertiesForTest(cellularNetwork);
          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          mojoApi_.setDeviceStateForTest({
            type: NetworkType.kCellular,
            deviceState: DeviceStateType.kEnabled,
            inhibitReason: InhibitReason.kNotInhibited,
            simInfos: [{
              iccid: test_iccid,
              isPrimary: true,
            }],
          });
          await flushAsync();
          assertFalse(internetDetailPage.showConfigurableSections_);
          // Check that an element from the primary account section exists.
          assertFalse(!!internetDetailPage.shadowRoot.querySelector(
              '#allowDataRoaming'));
          // The section ConnectDisconnect button belongs to should still be
          // showing.
          assertTrue(!!internetDetailPage.shadowRoot.querySelector(
              '#connectDisconnect'));
        });

    test(
        'Hide config section and Cellular Device object fields when' +
            'sim becomes non-active',
        async () => {
          init();
          const test_iccid = '11111111111111111';

          await mojoApi_.setNetworkTypeEnabledState(
              NetworkType.kCellular, true);
          const cellularNetwork = getManagedProperties(
              NetworkType.kCellular, 'cellular', OncSource.kDevice);
          cellularNetwork.typeProperties.cellular.iccid = test_iccid;

          const isShowingCellularDeviceObjectFields = () => {
            return internetDetailPage.shadowRoot.querySelector('#deviceFields')
                .fields.includes('cellular.homeProvider.name');
          };

          // Set sim to non-active.
          mojoApi_.setManagedPropertiesForTest(cellularNetwork);
          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          mojoApi_.setDeviceStateForTest({
            type: NetworkType.kCellular,
            deviceState: DeviceStateType.kEnabled,
            inhibitReason: InhibitReason.kNotInhibited,
            simInfos: [{
              iccid: test_iccid,
              isPrimary: false,
            }],
          });
          await flushAsync();
          assertFalse(internetDetailPage.showConfigurableSections_);
          assertFalse(isShowingCellularDeviceObjectFields());

          // Set sim to active.
          mojoApi_.setDeviceStateForTest({
            type: NetworkType.kCellular,
            deviceState: DeviceStateType.kEnabled,
            inhibitReason: InhibitReason.kNotInhibited,
            simInfos: [{
              iccid: test_iccid,
              isPrimary: true,
            }],
          });
          await flushAsync();
          assertTrue(internetDetailPage.showConfigurableSections_);
          assertTrue(isShowingCellularDeviceObjectFields());

          // Set sim to non-active again.
          mojoApi_.setDeviceStateForTest({
            type: NetworkType.kCellular,
            deviceState: DeviceStateType.kEnabled,
            inhibitReason: InhibitReason.kNotInhibited,
            simInfos: [{
              iccid: test_iccid,
              isPrimary: false,
            }],
          });
          await flushAsync();
          assertFalse(internetDetailPage.showConfigurableSections_);
          assertFalse(isShowingCellularDeviceObjectFields());
        });

    test('Do not show MAC address', async () => {
      const TEST_ICCID = '11111111111111111';
      const TEST_MAC_ADDRESS = '01:23:45:67:89:AB';
      const MISSING_MAC_ADDRESS = '00:00:00:00:00:00';

      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = true;
      cellularNetwork.typeProperties.cellular.simLocked = false;
      cellularNetwork.typeProperties.cellular.iccid = TEST_ICCID;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);
      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');

      let deviceState = {
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        inhibitReason: InhibitReason.kNotInhibited,
        simInfos: [{
          iccid: TEST_ICCID,
          isPrimary: true,
        }],
        macAddress: TEST_MAC_ADDRESS,
      };

      mojoApi_.setDeviceStateForTest(deviceState);
      await flushAsync();
      expandConfigurableSection();
      let macAddress =
          internetDetailPage.shadowRoot.querySelector('#mac-address-container');
      assertTrue(!!macAddress);
      assertFalse(macAddress.hidden);

      // Set MAC address to '00:00:00:00:00:00' missing address, this address
      // is provided when device MAC address cannot be retrieved. If this is the
      // case, the MAC address should not be displayed in UI.
      deviceState = {
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        inhibitReason: InhibitReason.kNotInhibited,
        simInfos: [{
          iccid: TEST_ICCID,
          isPrimary: true,
        }],
        macAddress: MISSING_MAC_ADDRESS,
      };
      mojoApi_.setDeviceStateForTest(deviceState);
      await flushAsync();
      expandConfigurableSection();
      macAddress =
          internetDetailPage.shadowRoot.querySelector('#mac-address-container');
      assertTrue(!!macAddress);
      assertTrue(macAddress.hidden);
    });

    test('Page disabled when inhibited', async () => {
      init();

      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork = getManagedProperties(
          NetworkType.kCellular, 'cellular', OncSource.kDevice);
      // Required for connectDisconnectButton to be rendered.
      cellularNetwork.connectionState = ConnectionStateType.kConnected;
      // Required for allowDataRoamingButton to be rendered.
      cellularNetwork.typeProperties.cellular.allowRoaming =
          OncMojo.createManagedBool(false);
      // Required for advancedFields to be rendered.
      cellularNetwork.typeProperties.cellular.networkTechnology = 'LTE';
      // Required for infoFields to be rendered.
      cellularNetwork.typeProperties.cellular.servingOperator = {name: 'name'};
      // Required for deviceFields to be rendered.
      const test_iccid = '11111111111111111';
      cellularNetwork.typeProperties.cellular.iccid = test_iccid;
      // Required for networkChooseMobile to be rendered.
      cellularNetwork.typeProperties.cellular.supportNetworkScan = true;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      // Start uninhibited.
      mojoApi_.setDeviceStateForTest({
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        inhibitReason: InhibitReason.kNotInhibited,
        // Required for configurable sections to be rendered.
        simInfos: [{
          iccid: test_iccid,
          isPrimary: true,
        }],
      });

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');

      await flushAsync();

      const connectDisconnectButton = getButton('connectDisconnect');
      const infoFields = getButton('infoFields');
      const cellularSimInfoAdvanced = getButton('cellularSimInfoAdvanced');
      const advancedFields = getButton('advancedFields');
      const deviceFields = getButton('deviceFields');
      const allowDataRoamingButton =
          internetDetailPage.shadowRoot
              .querySelector('cellular-roaming-toggle-button')
              .getCellularRoamingToggle();
      const networkChooseMobile =
          internetDetailPage.shadowRoot.querySelector('network-choose-mobile');
      const networkApnlist =
          internetDetailPage.shadowRoot.querySelector('network-apnlist');
      const networkIpConfig =
          internetDetailPage.shadowRoot.querySelector('network-ip-config');
      const networkNameservers =
          internetDetailPage.shadowRoot.querySelector('network-nameservers');
      const networkProxySection =
          internetDetailPage.shadowRoot.querySelector('network-proxy-section');

      assertFalse(connectDisconnectButton.disabled);
      assertFalse(allowDataRoamingButton.disabled);
      assertFalse(infoFields.disabled);
      assertFalse(cellularSimInfoAdvanced.disabled);
      assertFalse(advancedFields.disabled);
      assertFalse(deviceFields.disabled);
      assertFalse(networkChooseMobile.disabled);
      assertFalse(networkApnlist.disabled);
      assertFalse(networkIpConfig.disabled);
      assertFalse(networkNameservers.disabled);
      assertFalse(networkProxySection.disabled);

      // Mock device being inhibited.
      mojoApi_.setDeviceStateForTest({
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        inhibitReason: InhibitReason.kConnectingToProfile,
        simInfos: [{
          iccid: test_iccid,
          isPrimary: true,
        }],
      });
      await flushAsync();

      assertTrue(connectDisconnectButton.disabled);
      assertTrue(allowDataRoamingButton.disabled);
      assertTrue(infoFields.disabled);
      assertTrue(cellularSimInfoAdvanced.disabled);
      assertTrue(advancedFields.disabled);
      assertTrue(deviceFields.disabled);
      assertTrue(networkChooseMobile.disabled);
      assertTrue(networkApnlist.disabled);
      assertTrue(networkIpConfig.disabled);
      assertTrue(networkNameservers.disabled);
      assertTrue(networkProxySection.disabled);

      // Uninhibit.
      mojoApi_.setDeviceStateForTest({
        type: NetworkType.kCellular,
        deviceState: DeviceStateType.kEnabled,
        inhibitReason: InhibitReason.kNotInhibited,
        simInfos: [{
          iccid: test_iccid,
          isPrimary: true,
        }],
      });
      await flushAsync();

      assertFalse(connectDisconnectButton.disabled);
      assertFalse(allowDataRoamingButton.disabled);
      assertFalse(infoFields.disabled);
      assertFalse(cellularSimInfoAdvanced.disabled);
      assertFalse(advancedFields.disabled);
      assertFalse(deviceFields.disabled);
      assertFalse(networkChooseMobile.disabled);
      assertFalse(networkApnlist.disabled);
      assertFalse(networkIpConfig.disabled);
      assertFalse(networkNameservers.disabled);
      assertFalse(networkProxySection.disabled);
    });

    test('Cellular page disabled when blocked by policy', async () => {
      init();

      mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork = getManagedProperties(
          NetworkType.kCellular, 'cellular', OncSource.kDevice);
      // Required for connectDisconnectButton to be rendered.
      cellularNetwork.connectionState = ConnectionStateType.kNotConnected;
      cellularNetwork.typeProperties.cellular.allowRoaming =
          OncMojo.createManagedBool(false);
      // Required for advancedFields to be rendered.
      cellularNetwork.typeProperties.cellular.networkTechnology = 'LTE';
      // Required for infoFields to be rendered.
      cellularNetwork.typeProperties.cellular.servingOperator = {name: 'name'};
      // Required for deviceFields to be rendered.
      const test_iccid = '11111111111111111';
      cellularNetwork.typeProperties.cellular.iccid = test_iccid;
      cellularNetwork.typeProperties.cellular.supportNetworkScan = true;
      cellularNetwork.source = OncSource.kNone;
      mojoApi_.setManagedPropertiesForTest(cellularNetwork);

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      internetDetailPage.globalPolicy = {
        allowOnlyPolicyCellularNetworks: true,
      };
      await flushAsync();

      const connectDisconnectButton = getButton('connectDisconnect');
      assertTrue(connectDisconnectButton.hidden);
      assertTrue(connectDisconnectButton.disabled);
      assertFalse(!!internetDetailPage.shadowRoot.querySelector('#infoFields'));
      const cellularSimInfoAdvanced = getButton('cellularSimInfoAdvanced');
      assertFalse(cellularSimInfoAdvanced.disabled);
      assertFalse(cellularSimInfoAdvanced.hidden);
      const advancedFields = getButton('advancedFields');
      assertFalse(advancedFields.disabled);
      assertFalse(advancedFields.hidden);
      const deviceFields = getButton('deviceFields');
      assertFalse(deviceFields.disabled);
      assertFalse(deviceFields.hidden);

      assertFalse(!!internetDetailPage.shadowRoot.querySelector(
          'cellular-roaming-toggle-button'));
      assertFalse(!!internetDetailPage.shadowRoot.querySelector(
          'network-choose-mobile'));
      assertFalse(
          !!internetDetailPage.shadowRoot.querySelector('network-apnlist'));
      assertFalse(
          !!internetDetailPage.shadowRoot.querySelector('network-ip-config'));
      assertFalse(
          !!internetDetailPage.shadowRoot.querySelector('network-nameservers'));
      assertFalse(!!internetDetailPage.shadowRoot.querySelector(
          'network-proxy-section'));
    });
    // Syntactic sugar for running test twice with different values for the
    // apnRevamp feature flag.
    [true, false].forEach(isApnRevampEnabled => {
      test('Show/Hide APN row correspondingly to ApnRevamp flag', async () => {
        loadTimeData.overrideValues({isApnRevampEnabled});
        init();
        mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
        const apnName = 'test';
        const cellularNetwork =
            getManagedProperties(NetworkType.kCellular, 'cellular');
        cellularNetwork.typeProperties.cellular.connectedApn = {};
        cellularNetwork.typeProperties.cellular.connectedApn.accessPointName =
            apnName;
        mojoApi_.setManagedPropertiesForTest(cellularNetwork);
        internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
        await flushAsync();
        const crLink =
            internetDetailPage.shadowRoot.querySelector('#apnSubpageButton');
        const apn =
            crLink ? crLink.shadowRoot.querySelector('#subLabel') : null;
        if (isApnRevampEnabled) {
          assertTrue(!!apn);
          assertEquals(apn.textContent.trim(), apnName);
        } else {
          assertFalse(!!apn);
        }
      });
    });
  });

  suite('DetailsPageEthernet', function() {
    test('LoadPage', function() {
      init();
    });

    test('Eth1', function() {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kEthernet, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kEthernet, 'eth1'),
      ]);

      internetDetailPage.init('eth1_guid', 'Ethernet', 'eth1');
      assertEquals('eth1_guid', internetDetailPage.guid);
      return flushAsync().then(() => {
        return mojoApi_.whenCalled('getManagedProperties');
      });
    });

    test('Deep link to configure ethernet button', async () => {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kEthernet, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kEthernet, 'eth1'),
      ]);

      const params = new URLSearchParams();
      params.append('guid', 'eth1_guid');
      params.append('type', 'Ethernet');
      params.append('name', 'eth1');
      params.append('settingId', '0');
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await flushAsync();

      const deepLinkElement = getButton('configureButton');
      await waitAfterNextRender(deepLinkElement);

      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Configure ethernet button should be focused for settingId=0.');
    });
  });

  suite('DetailsPageTether', function() {
    test('LoadPage', function() {
      init();
    });

    test(
        'Create tether network, first connection attempt shows tether dialog',
        async () => {
          init();
          mojoApi_.setNetworkTypeEnabledState(NetworkType.kTether, true);
          setNetworksForTest([
            OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
          ]);

          internetDetailPage.init('tether1_guid', 'Tether', 'tether1');
          assertEquals('tether1_guid', internetDetailPage.guid);
          await flushAsync();
          await mojoApi_.whenCalled('getManagedProperties');

          const connect =
              internetDetailPage.shadowRoot.querySelector('#connectDisconnect');
          assertTrue(!!connect);

          const tetherDialog =
              internetDetailPage.shadowRoot.querySelector('#tetherDialog');
          assertTrue(!!tetherDialog);
          assertFalse(tetherDialog.$.dialog.open);

          let showTetherDialogFinished;
          const showTetherDialogPromise = new Promise((resolve) => {
            showTetherDialogFinished = resolve;
          });
          const showTetherDialog = internetDetailPage.showTetherDialog_;
          internetDetailPage.showTetherDialog_ = () => {
            showTetherDialog.call(internetDetailPage);
            showTetherDialogFinished();
          };

          connect.click();
          await showTetherDialogPromise;
          assertTrue(tetherDialog.$.dialog.open);
        });

    test('Deep link to disconnect tether network', async () => {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kTether, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
      ]);
      const tetherNetwork =
          getManagedProperties(NetworkType.kTether, 'tether1');
      tetherNetwork.connectable = true;
      mojoApi_.setManagedPropertiesForTest(tetherNetwork);

      await flushAsync();

      const params = new URLSearchParams();
      params.append('guid', 'tether1_guid');
      params.append('type', 'Tether');
      params.append('name', 'tether1');
      params.append('settingId', '23');
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await flushAsync();

      const deepLinkElement =
          getButton('connectDisconnect').shadowRoot.querySelector('cr-button');
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Disconnect tether button should be focused for settingId=23.');
    });
  });

  suite('DetailsPageAutoConnect', function() {
    test('Auto Connect toggle updates after GUID change', function() {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifi1 =
          getManagedProperties(NetworkType.kWiFi, 'wifi1', OncSource.kDevice);
      wifi1.typeProperties.wifi.autoConnect = OncMojo.createManagedBool(true);
      mojoApi_.setManagedPropertiesForTest(wifi1);

      const wifi2 =
          getManagedProperties(NetworkType.kWiFi, 'wifi2', OncSource.kDevice);
      wifi2.typeProperties.wifi.autoConnect = OncMojo.createManagedBool(false);
      mojoApi_.setManagedPropertiesForTest(wifi2);

      internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi1');
      return flushAsync()
          .then(() => {
            const toggle = internetDetailPage.shadowRoot.querySelector(
                '#autoConnectToggle');
            assertTrue(!!toggle);
            assertTrue(toggle.checked);
            internetDetailPage.init('wifi2_guid', 'WiFi', 'wifi2');
            return flushAsync();
          })
          .then(() => {
            const toggle = internetDetailPage.shadowRoot.querySelector(
                '#autoConnectToggle');
            assertTrue(!!toggle);
            assertFalse(toggle.checked);
          });
    });

    test('Auto Connect updates don\'t trigger a re-save', function() {
      init();
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      let wifi1 =
          getManagedProperties(NetworkType.kWiFi, 'wifi1', OncSource.kDevice);
      wifi1.typeProperties.wifi.autoConnect = OncMojo.createManagedBool(true);
      mojoApi_.setManagedPropertiesForTest(wifi1);

      mojoApi_.whenCalled('setProperties').then(() => assert(false));
      internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi1');
      internetDetailPage.onNetworkStateChanged({guid: 'wifi1_guid'});
      return flushAsync()
          .then(() => {
            const toggle = internetDetailPage.shadowRoot.querySelector(
                '#autoConnectToggle');
            assertTrue(!!toggle);
            assertTrue(toggle.checked);

            // Rebuild the object to force polymer to recognize a change.
            wifi1 = getManagedProperties(
                NetworkType.kWiFi, 'wifi1', OncSource.kDevice);
            wifi1.typeProperties.wifi.autoConnect =
                OncMojo.createManagedBool(false);
            mojoApi_.setManagedPropertiesForTest(wifi1);
            internetDetailPage.onNetworkStateChanged({guid: 'wifi1_guid'});
            return flushAsync();
          })
          .then(() => {
            const toggle = internetDetailPage.shadowRoot.querySelector(
                '#autoConnectToggle');
            assertTrue(!!toggle);
            assertFalse(toggle.checked);
          });
    });
  });
});
