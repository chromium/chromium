// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CellularRoamingToggleButtonElement, NetworkProxySectionElement, PasspointRemoveDialogElement, SettingsInternetDetailPageElement} from 'chrome://os-settings/lazy_load.js';
import {CrDialogElement, CrLinkRowElement, InternetPageBrowserProxyImpl, LocalizedLinkElement, Router, routes, settingMojom, SettingsToggleButtonElement, setUserActionRecorderForTesting, userActionRecorderMojom} from 'chrome://os-settings/os_settings.js';
import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {PasspointSubscription} from 'chrome://resources/ash/common/connectivity/passpoint.mojom-webui.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkApnListElement} from 'chrome://resources/ash/common/network/network_apnlist.js';
import {NetworkChooseMobileElement} from 'chrome://resources/ash/common/network/network_choose_mobile.js';
import {NetworkConfigToggleElement} from 'chrome://resources/ash/common/network/network_config_toggle.js';
import {NetworkIpConfigElement} from 'chrome://resources/ash/common/network/network_ip_config.js';
import {NetworkNameserversElement} from 'chrome://resources/ash/common/network/network_nameservers.js';
import {NetworkPropertyListMojoElement} from 'chrome://resources/ash/common/network/network_property_list_mojo.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {ActivationStateType, ApnAuthenticationType, ApnIpType, ApnSource, ApnState, DeviceStateProperties, GlobalPolicy, InhibitReason, ManagedOpenVPNProperties, ManagedProperties, MatchType, NetworkStateProperties, ProxyMode, SuppressionType, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, IPConfigType, NetworkType, OncSource, PolicySource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakePasspointService} from 'chrome://webui-test/chromeos/fake_passpoint_service_mojom.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {FakeUserActionRecorder} from '../fake_user_action_recorder.js';

import {TestInternetPageBrowserProxy} from './test_internet_page_browser_proxy.js';

suite('<settings-internet-detail-subpage>', () => {
  let internetDetailPage: SettingsInternetDetailPageElement;
  let mojoApi: FakeNetworkConfig;
  let passpointServiceApi: FakePasspointService;
  let browserProxy: TestInternetPageBrowserProxy;
  let userActionRecorder: userActionRecorderMojom.UserActionRecorderInterface;
  const CELLULAR_SIM_LOCK_SETTING =
      settingMojom.Setting.kCellularSimLock.toString();

  const PREFS = {
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
    // triggering a change in PREFS without it will fail a "Pref is missing"
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

  suiteSetup(() => {
    mojoApi = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi);
    passpointServiceApi = new FakePasspointService();
    MojoConnectivityProvider.getInstance().setPasspointServiceForTest(
        passpointServiceApi);
  });

  function setNetworksForTest(networks: NetworkStateProperties[]): void {
    mojoApi.resetForTest();
    mojoApi.addNetworksForTest(networks);
  }

  function setSubscriptionForTest(subscription: PasspointSubscription): void {
    passpointServiceApi.resetForTest();
    passpointServiceApi.addSubscription(subscription);
  }

  function getAllowSharedProxy(): SettingsToggleButtonElement {
    const allowShared =
        internetDetailPage.shadowRoot!.querySelector('network-proxy-section')!
            .shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#allowShared');
    assertTrue(!!allowShared);
    return allowShared;
  }

  function getButton(buttonId: string): HTMLButtonElement {
    const button =
        internetDetailPage.shadowRoot!.querySelector<HTMLButtonElement>(
            `#${buttonId}`);
    assertTrue(!!button);
    return button;
  }

  function getPasspointRemoveDialog(): PasspointRemoveDialogElement {
    const dialog = internetDetailPage.shadowRoot!
                       .querySelector<PasspointRemoveDialogElement>(
                           '#passpointRemovalDialog');
    assertTrue(!!dialog);
    return dialog;
  }

  function getManagedProperties(
      networkType: NetworkType, name: string,
      source?: OncSource): ManagedProperties {
    const result =
        OncMojo.getDefaultManagedProperties(networkType, name + '_guid', name);
    if (source) {
      result.source = source;
    }
    return result;
  }

  function getHiddenToggle(): SettingsToggleButtonElement|null {
    return internetDetailPage.shadowRoot!.querySelector('#hiddenToggle');
  }

  /**
   * @param doNotProvidePrefs If provided, determine whether
   *     prefs should be provided for the element.
   */
  function init(doNotProvidePrefs?: boolean): void {
    internetDetailPage =
        document.createElement('settings-internet-detail-subpage');
    assertTrue(!!internetDetailPage);
    if (!doNotProvidePrefs) {
      internetDetailPage.prefs = PREFS;
    }
    document.body.appendChild(internetDetailPage);
  }

  function getDefaultDeviceStateProps(): DeviceStateProperties {
    return {
      ipv4Address: undefined,
      ipv6Address: undefined,
      imei: undefined,
      macAddress: undefined,
      scanning: false,
      simLockStatus: undefined,
      simInfos: undefined,
      inhibitReason: InhibitReason.kNotInhibited,
      simAbsent: false,
      deviceState: DeviceStateType.kUninitialized,
      type: NetworkType.kCellular,
      managedNetworkAvailable: false,
      serial: undefined,
      isCarrierLocked: false,
      isFlashing: false,
    };
  }

  async function deepLinkToSimLockElement(isSimLocked: boolean): Promise<void> {
    init();

    const TEST_ICCID = '11111111111111111';
    mojoApi.setDeviceStateForTest({
      ...getDefaultDeviceStateProps(),
      deviceState: DeviceStateType.kEnabled,
      simLockStatus: {
        lockEnabled: true,
        lockType: isSimLocked ? 'sim-pin' : '',
        retriesLeft: 0,
      },
      simInfos: [{
        iccid: TEST_ICCID,
        isPrimary: true,
        slotId: 0,
        eid: '',
      }],
    });

    const cellularNetwork =
        getManagedProperties(NetworkType.kCellular, 'cellular');
    cellularNetwork.connectable = false;
    cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
    mojoApi.setManagedPropertiesForTest(cellularNetwork);

    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    params.append('type', 'Cellular');
    params.append('name', 'cellular');
    params.append('settingId', CELLULAR_SIM_LOCK_SETTING);
    Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

    await flushTasks();
  }

  setup(async () => {
    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);

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
    });

    mojoApi.resetForTest();

    browserProxy = new TestInternetPageBrowserProxy();
    InternetPageBrowserProxyImpl.setInstance(browserProxy);

    await flushTasks();
  });

  teardown(() => {
    internetDetailPage.close();
    internetDetailPage.remove();
    browserProxy.reset();
    mojoApi.resetForTest();
    passpointServiceApi.resetForTest();
    Router.getInstance().resetRouteForTesting();
  });

  function getDefaultGlobalPolicy(): GlobalPolicy {
    return {
      allowApnModification: false,
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
      allowTextMessages: SuppressionType.kUnset,
    };
  }

  suite('DetailsPageWiFi', () => {
    test('LoadPage', () => {
      init();
    });

    test('WiFi1', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1'),
      ]);

      internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi1');
      assertEquals('wifi1_guid', internetDetailPage.guid);
      await flushTasks();
      await mojoApi.whenCalled('getManagedProperties');
    });

    // Sanity test for the suite setup. Makes sure that re-opening the details
    // page with a different network also succeeds.
    test('WiFi2', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2'),
      ]);

      internetDetailPage.init('wifi2_guid', 'WiFi', 'wifi2');
      assertEquals('wifi2_guid', internetDetailPage.guid);
      await flushTasks();
      await mojoApi.whenCalled('getManagedProperties');
    });

    test('Connect button disabled when WiFi is out of range', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork =
          getManagedProperties(NetworkType.kWiFi, 'out_of_range_wifi');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      mojoApi.setManagedPropertiesForTest(wifiNetwork);
      mojoApi.setWifiNetworkVisibleForTest('out_of_range_wifi_guid', false);

      internetDetailPage.init(
          'out_of_range_wifi_guid', 'WiFi', 'out_of_range_wifi');
      await flushTasks();
      const connectButton = getButton('connectDisconnect');
      assertFalse(connectButton.hidden);
      assertTrue(connectButton.disabled);
    });

    test(
        'Connect button enabled on hidden network even when WiFi is out' +
            ' of range',
        async () => {
          init();
          mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
          const wifiNetwork = getManagedProperties(
              NetworkType.kWiFi, 'out_of_range_hidden_wifi');
          wifiNetwork.source = OncSource.kUser;
          wifiNetwork.connectable = true;
          wifiNetwork.typeProperties.wifi!.hiddenSsid =
              OncMojo.createManagedBool(true);
          mojoApi.setManagedPropertiesForTest(wifiNetwork);
          mojoApi.setWifiNetworkVisibleForTest(
              'out_of_range_hidden_wifi_guid', false);

          internetDetailPage.init(
              'out_of_range_hidden_wifi_guid', 'WiFi',
              'out_of_range_hidden_wifi');
          await flushTasks();
          const connectButton = getButton('connectDisconnect');
          assertFalse(connectButton.hidden);
          assertFalse(connectButton.disabled);
        });

    test('WiFi in a portal portalState', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kPortal;

      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      await flushTasks();
      const networkStateText =
          internetDetailPage.shadowRoot!.querySelector('#networkState');
      assertTrue(!!networkStateText);
      assertTrue(networkStateText.hasAttribute('warning'));
      assertEquals(
          internetDetailPage.i18n('networkListItemSignIn'),
          networkStateText.textContent!.trim());
      const signinButton = getButton('signinButton');
      assertTrue(!!signinButton);
      assertFalse(signinButton.hidden);
      assertFalse(signinButton.disabled);
    });

    test('WiFi in a portal-suspected portalState', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kPortalSuspected;

      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      await flushTasks();
      const networkStateText =
          internetDetailPage.shadowRoot!.querySelector('#networkState');
      assertTrue(!!networkStateText);
      assertTrue(networkStateText.hasAttribute('warning'));
      assertEquals(
          internetDetailPage.i18n('networkListItemSignIn'),
          networkStateText.textContent!.trim());
      const signinButton = getButton('signinButton');
      assertTrue(!!signinButton);
      assertFalse(signinButton.hidden);
      assertFalse(signinButton.disabled);
    });

    test('WiFi in a no internet portalState', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.connectionState = ConnectionStateType.kPortal;
      wifiNetwork.portalState = PortalState.kNoInternet;

      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      await flushTasks();
      const networkStateText =
          internetDetailPage.shadowRoot!.querySelector('#networkState');
      assertTrue(!!networkStateText);
      assertTrue(networkStateText.hasAttribute('warning'));
      assertEquals(
          internetDetailPage.i18n('networkListItemConnectedNoConnectivity'),
          networkStateText.textContent!.trim());
      const signinButton = getButton('signinButton');
      assertTrue(!!signinButton);
      assertTrue(signinButton.hidden);
      assertTrue(signinButton.disabled);
    });

    test('Hidden toggle enabled', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.typeProperties.wifi!.hiddenSsid =
          OncMojo.createManagedBool(true);

      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      await flushTasks();
      const hiddenToggle = getHiddenToggle();
      assertTrue(!!hiddenToggle);
      assertTrue(hiddenToggle.checked);
    });

    test('Hidden toggle disabled', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.typeProperties.wifi!.hiddenSsid =
          OncMojo.createManagedBool(false);

      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      await flushTasks();
      const hiddenToggle = getHiddenToggle();
      assertTrue(!!hiddenToggle);
      assertFalse(hiddenToggle.checked);
    });

    test('Hidden toggle hidden when not configured', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.connectable = false;
      wifiNetwork.typeProperties.wifi!.hiddenSsid =
          OncMojo.createManagedBool(false);

      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      await flushTasks();
      const hiddenToggle = getHiddenToggle();
      assertNull(hiddenToggle);
    });

    test('Hidden toggle hidden for non-WiFi networks', async () => {
      init();
      for (const networkType
               of [NetworkType.kCellular, NetworkType.kEthernet,
                   NetworkType.kTether, NetworkType.kVPN]) {
        mojoApi.setNetworkTypeEnabledState(networkType, true);
        const networkTypeString = OncMojo.getNetworkTypeString(networkType);
        const networkGuid = 'network_guid_' + networkTypeString;
        const networkName = 'network_name_' + networkTypeString;
        const network = getManagedProperties(networkType, networkName);

        mojoApi.setManagedPropertiesForTest(network);

        internetDetailPage.init(networkGuid, networkTypeString, networkName);
        await flushTasks();
        const hiddenToggle = getHiddenToggle();
        assertNull(hiddenToggle);
      }
    });

    test('Proxy Unshared', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      await flushTasks();
      const proxySection =
          internetDetailPage.shadowRoot!.querySelector('network-proxy-section');
      assertTrue(!!proxySection);
      const allowShared =
          proxySection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#allowShared');
      assertTrue(!!allowShared);
      assertTrue(allowShared.hidden);
    });

    test('Proxy Shared', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          NetworkType.kWiFi, 'wifi_device', OncSource.kDevice);
      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_device_guid', 'WiFi', 'wifi_device');
      await flushTasks();
      const allowShared = getAllowSharedProxy();
      assertFalse(allowShared.hidden);
      assertFalse(allowShared.disabled);
    });

    // When proxy settings are managed by a user policy but the configuration
    // is from the shared (device) profile, they still respect the
    // allowed_shared_proxies pref so #allowShared should be visible.
    // TODO(stevenjb): Improve this: crbug.com/662529.
    test('Proxy Shared User Managed', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          NetworkType.kWiFi, 'wifi_device', OncSource.kDevice);
      wifiNetwork.proxySettings = {
        type: {
          activeValue: 'Manual',
          policySource: PolicySource.kUserPolicyEnforced,
          policyValue: '',
        },
        manual: undefined,
        excludeDomains: undefined,
        pac: undefined,
      };
      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_device_guid', 'WiFi', 'wifi_device');
      await flushTasks();
      const allowShared = getAllowSharedProxy();
      assertFalse(allowShared.hidden);
      assertFalse(allowShared.disabled);
    });

    // When proxy settings are managed by a device policy they may respect the
    // allowd_shared_proxies pref so #allowShared should be visible.
    test('Proxy Shared Device Managed', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          NetworkType.kWiFi, 'wifi_device', OncSource.kDevice);
      wifiNetwork.proxySettings = {
        type: {
          activeValue: 'Manual',
          policySource: PolicySource.kDevicePolicyEnforced,
          policyValue: '',
        },
        manual: undefined,
        excludeDomains: undefined,
        pac: undefined,
      };
      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_device_guid', 'WiFi', 'wifi_device');
      await flushTasks();
      const allowShared = getAllowSharedProxy();
      assertFalse(allowShared.hidden);
      assertFalse(allowShared.disabled);
    });

    // Tests that when the route changes to one containing a deep link to
    // the shared proxy toggle, toggle is focused.
    test('Deep link to shared proxy toggle', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(
          NetworkType.kWiFi, 'wifi_device', OncSource.kDevice);
      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      const WIFI_PROXY_SETTING = settingMojom.Setting.kWifiProxy.toString();
      const params = new URLSearchParams();
      params.append('guid', 'wifi_device_guid');
      params.append('type', 'WiFi');
      params.append('name', 'wifi_device');
      params.append('settingId', WIFI_PROXY_SETTING);
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);
      await flushTasks();

      const networkProxyElement =
          internetDetailPage.shadowRoot!.querySelector('network-proxy-section');
      assertTrue(!!networkProxyElement);
      const allowShared =
          networkProxyElement.shadowRoot!.querySelector('#allowShared');
      assertTrue(!!allowShared);
      const deepLinkElement =
          allowShared.shadowRoot!.querySelector<HTMLElement>('#control');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Allow shared proxy toggle should be focused for settingId=${
              WIFI_PROXY_SETTING}.`);

      // Close the page to ensure the test is fully cleaned up and wait for
      // os_route's popstate listener to fire. If we don't add this wait, this
      // event can fire during the other tests which may interfere with its
      // routing.
      const popStatePromise = eventToPromise('popstate', window);
      internetDetailPage.close();
      await popStatePromise;
    });

    test('WiFi page disabled when blocked by policy', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi_user');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_user_guid', 'WiFi', 'wifi_user');
      internetDetailPage.globalPolicy = {
        ...getDefaultGlobalPolicy(),
        allowOnlyPolicyWifiNetworksToConnect: true,
      };
      await flushTasks();

      const connectDisconnectButton = getButton('connectDisconnect');
      assertTrue(connectDisconnectButton.hidden);
      assertTrue(connectDisconnectButton.disabled);
      assertNull(internetDetailPage.shadowRoot!.querySelector('#infoFields'));
      const configureButton = getButton('configureButton');
      assertTrue(configureButton.hidden);
      const advancedFields = getButton('advancedFields');
      assertFalse(advancedFields.disabled);
      assertFalse(advancedFields.hidden);
      assertNull(internetDetailPage.shadowRoot!.querySelector('#deviceFields'));
      assertNull(
          internetDetailPage.shadowRoot!.querySelector('network-ip-config'));
      assertNull(
          internetDetailPage.shadowRoot!.querySelector('network-nameservers'));
      assertNull(internetDetailPage.shadowRoot!.querySelector(
          'network-proxy-section'));
    });

    test('WiFi Passpoint removal leads to subscription page', async () => {
      init();

      const subId = 'a_passpoint_id';
      setSubscriptionForTest({
        id: subId,
        friendlyName: 'My Passpoint provider',
        domains: [],
        provisioningSource: '',
        expirationEpochMs: 0n,
        trustedCa: null,
      });

      mojoApi.resetForTest();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork =
          getManagedProperties(NetworkType.kWiFi, 'wifi_passpoint');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.typeProperties.wifi!.passpointId = subId;
      wifiNetwork.typeProperties.wifi!.passpointMatchType = MatchType.kHome;
      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_passpoint_guid', 'WiFi', 'wifi_passpoint');
      await flushTasks();

      const forgetButton = getButton('forgetButton');
      assertFalse(forgetButton.hidden);
      assertFalse(forgetButton.disabled);

      // Click the button and check the dialog is displayed.
      forgetButton.click();
      await waitAfterNextRender(forgetButton);
      const removeDialog = getPasspointRemoveDialog();
      let dialogElement =
          removeDialog.shadowRoot!.querySelector<CrDialogElement>('#dialog');
      assertTrue(!!dialogElement);
      assertTrue(dialogElement.open);

      // Check "Confirm" leads to Passpoint subscription page.
      forgetButton.click();
      await flushTasks();
      dialogElement =
          removeDialog.shadowRoot!.querySelector<CrDialogElement>('#dialog');
      assertTrue(!!dialogElement);
      assertTrue(dialogElement.open);
      const confirmButton =
          removeDialog.shadowRoot!.querySelector<HTMLButtonElement>(
              '#confirmButton');
      assertTrue(!!confirmButton);
      const showDetailPromise = eventToPromise('show-passpoint-detail', window);
      confirmButton.click();
      await flushTasks();
      const showDetailEvent = await showDetailPromise;
      assertEquals(subId, showDetailEvent.detail.id);
    });

    test(
        'WiFi network removal without Passpoint does not show a dialog',
        async () => {
          init();
          mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
          const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi');
          wifiNetwork.source = OncSource.kUser;
          wifiNetwork.connectable = true;
          mojoApi.setManagedPropertiesForTest(wifiNetwork);

          internetDetailPage.init('wifi_guid', 'WiFi', 'wifi');
          await flushTasks();

          const forgetButton = getButton('forgetButton');
          assertFalse(forgetButton.hidden);
          assertFalse(forgetButton.disabled);

          // Click the button and check the dialog is displayed.
          forgetButton.click();
          await flushTasks();
          assertNull(internetDetailPage.shadowRoot!.querySelector(
              '#passpointRemovalDialog'));
        });

    test('WiFi network with Passpoint shows provider row', async () => {
      init();

      const subId = 'a_passpoint_id';
      setSubscriptionForTest({
        id: subId,
        friendlyName: 'My Passpoint provider',
        domains: [],
        provisioningSource: '',
        expirationEpochMs: 0n,
        trustedCa: null,
      });
      mojoApi.resetForTest();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork =
          getManagedProperties(NetworkType.kWiFi, 'wifi_passpoint');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.typeProperties.wifi!.passpointId = subId;
      wifiNetwork.typeProperties.wifi!.passpointMatchType = MatchType.kHome;
      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_passpoint_guid', 'WiFi', 'wifi_passpoint');
      await flushTasks();

      const row =
          internetDetailPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#passpointProviderRow');
      // The row is present only when Passpoint is enabled.
      assertTrue(!!row);

      const showDetailPromise = eventToPromise('show-passpoint-detail', window);
      assertTrue(!!row);
      row.click();
      const showDetailEvent = await showDetailPromise;
      assertEquals(subId, showDetailEvent.detail.id);
    });

    test(
        'WiFi network without Passpoint does not show provider row',
        async () => {
          init();
          mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
          const wifiNetwork = getManagedProperties(NetworkType.kWiFi, 'wifi');
          wifiNetwork.source = OncSource.kUser;
          wifiNetwork.connectable = true;
          mojoApi.setManagedPropertiesForTest(wifiNetwork);

          internetDetailPage.init('wifi_guid', 'WiFi', 'wifi');
          await flushTasks();

          assertNull(internetDetailPage.shadowRoot!.querySelector(
              '#passpointProviderRow'));
        });

    test('WiFi network with Passpoint has no configure button', async () => {
      init();

      const subId = 'a_passpoint_id';
      setSubscriptionForTest({
        id: subId,
        friendlyName: 'My Passpoint provider',
        domains: [],
        provisioningSource: '',
        expirationEpochMs: 0n,
        trustedCa: null,
      });
      mojoApi.resetForTest();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifiNetwork =
          getManagedProperties(NetworkType.kWiFi, 'wifi_passpoint');
      wifiNetwork.source = OncSource.kUser;
      wifiNetwork.connectable = true;
      wifiNetwork.typeProperties.wifi!.passpointId = subId;
      wifiNetwork.typeProperties.wifi!.passpointMatchType = MatchType.kHome;
      mojoApi.setManagedPropertiesForTest(wifiNetwork);

      internetDetailPage.init('wifi_passpoint_guid', 'WiFi', 'wifi_passpoint');
      await flushTasks();

      const configureButton = getButton('configureButton');
      assertTrue(configureButton.hidden);
    });
  });

  suite('DetailsPageVPN', () => {
    /**
     * @param doNotProvidePrefs If provided, determine whether
     *     prefs should be provided for the element.
     */
    function initVpn(doNotProvidePrefs?: boolean): void {
      init(doNotProvidePrefs);
      mojoApi.setNetworkTypeEnabledState(NetworkType.kVPN, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kVPN, 'vpn1'),
      ]);

      internetDetailPage.init('vpn1_guid', 'VPN', 'vpn1');
    }

    /**
     * @param managedProperties
     *     Managed properties used to initialize the network.
     */
    function initManagedVpn(managedProperties: ManagedProperties): void {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kVPN, true);
      mojoApi.resetForTest();
      mojoApi.addNetworksForTest([
        OncMojo.managedPropertiesToNetworkState(managedProperties),
      ]);
      mojoApi.setManagedPropertiesForTest(managedProperties);
      internetDetailPage.init(
          managedProperties.guid, 'VPN', managedProperties.name!.activeValue);
    }

    function getDefaultManagedOpenVpnProps(): ManagedOpenVPNProperties {
      return {
        auth: undefined,
        authRetry: undefined,
        authNoCache: undefined,
        cipher: undefined,
        clientCertPkcs11Id: undefined,
        clientCertPattern: undefined,
        clientCertProvisioningProfileId: undefined,
        clientCertRef: undefined,
        clientCertType: undefined,
        compressionAlgorithm: undefined,
        extraHosts: undefined,
        ignoreDefaultRoute: undefined,
        keyDirection: undefined,
        nsCertType: undefined,
        password: undefined,
        port: undefined,
        proto: undefined,
        pushPeerInfo: undefined,
        remoteCertEku: undefined,
        remoteCertKu: undefined,
        remoteCertTls: undefined,
        renegSec: undefined,
        saveCredentials: undefined,
        serverCaPems: undefined,
        serverCaRefs: undefined,
        serverCertRef: undefined,
        serverPollTimeout: undefined,
        shaper: undefined,
        staticChallenge: undefined,
        tlsAuthContents: undefined,
        tlsRemote: undefined,
        tlsVersionMin: undefined,
        userAuthenticationType: undefined,
        username: undefined,
        verb: undefined,
        verifyHash: undefined,
        verifyX509: undefined,
      };
    }

    /**
     * @param oncSource If
     *     provided, sets the source (user / device / policy) of the network.
     */
    function initAdvancedVpn(oncSource: OncSource = 0): void {
      const defaultManagedStringProps = {
        activeValue: '',
        policySource: PolicySource.kNone,
        policyValue: undefined,
      };
      const vpn1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'vpn1_guid', 'vpn1');
      vpn1.source = oncSource;
      vpn1.typeProperties.vpn!.type = VpnType.kOpenVPN;
      vpn1.typeProperties.vpn!.openVpn = {
        ...getDefaultManagedOpenVpnProps(),
        auth: {
          ...defaultManagedStringProps,
          activeValue: 'MD5',
        },
        cipher: {
          ...defaultManagedStringProps,
          activeValue: 'AES-192-CBC',
        },
        compressionAlgorithm: {
          ...defaultManagedStringProps,
          activeValue: 'LZO',
        },
        tlsAuthContents: {
          ...defaultManagedStringProps,
          activeValue: 'FAKE_CREDENTIAL_VPaJDV9x',
        },
        keyDirection: {
          ...defaultManagedStringProps,
          activeValue: '1',
        },
      };
      initManagedVpn(vpn1);
    }

    function initVpnWithNoAdvancedProperties(): void {
      const vpn1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'vpn1_guid', 'vpn1');
      vpn1.source = OncSource.kUserPolicy;
      vpn1.typeProperties.vpn!.type = VpnType.kOpenVPN;
      // Try out all the values considered "empty" to make sure we do not
      // register any of them as set.
      vpn1.typeProperties.vpn!.openVpn = {
        ...getDefaultManagedOpenVpnProps(),
      };
      initManagedVpn(vpn1);
    }

    function initWireGuard(): void {
      const wg1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'wg1_guid', 'wg1');
      wg1.typeProperties.vpn!.type = VpnType.kWireGuard;
      wg1.typeProperties.vpn!.wireguard = {
        peers: {
          activeValue: [{
            publicKey: 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=',
            endpoint: '192.168.66.66:32000',
            allowedIps: '0.0.0.0/0',
            presharedKey: '',
            persistentKeepaliveInterval: 0,
          }],
          policySource: PolicySource.kNone,
          policyValue: [],
        },
        ipAddresses: undefined,
        privateKey: undefined,
        publicKey: undefined,
      };
      wg1.staticIpConfig = {
        ipAddress: {
          activeValue: '10.10.0.1',
          policySource: PolicySource.kNone,
          policyValue: '',
        },
        gateway: undefined,
        nameServers: undefined,
        routingPrefix: undefined,
        type: IPConfigType.kIPv4,
        webProxyAutoDiscoveryUrl: undefined,
      };
      initManagedVpn(wg1);
    }

    test('VPN config allowed', async () => {
      initVpn();
      PREFS.vpn_config_allowed.value = true;
      internetDetailPage.prefs = PREFS;
      await flushTasks();
      const disconnectButton = getButton('connectDisconnect');
      assertFalse(disconnectButton.hasAttribute('enforced_'));
      assertNull(disconnectButton.shadowRoot!.querySelector(
          'cr-policy-pref-indicator'));
    });

    test('VPN config disallowed', async () => {
      initVpn();
      PREFS.vpn_config_allowed.value = false;
      internetDetailPage.prefs = PREFS;
      await flushTasks();
      const disconnectButton = getButton('connectDisconnect');
      assertTrue(disconnectButton.hasAttribute('enforced_'));
      assertTrue(!!disconnectButton.shadowRoot!.querySelector(
          'cr-policy-pref-indicator'));
    });

    test('Managed VPN with advanced fields', async () => {
      initAdvancedVpn(OncSource.kUserPolicy);
      await flushTasks();
      assertTrue(
          !!internetDetailPage.shadowRoot!.querySelector('#advancedFields'));
    });

    test('Unmanaged VPN with advanced fields', async () => {
      initAdvancedVpn(OncSource.kUser);
      await flushTasks();
      assertNull(
          internetDetailPage.shadowRoot!.querySelector('#advancedFields'));
    });

    // Regression test for issue fixed as part of https://crbug.com/1191626
    // where page would throw an exception if prefs were undefined. Prefs are
    // expected to be undefined if InternetDetailPage is loaded directly (e.g.,
    // when the user clicks on the network in Quick Settings).
    test('VPN without prefs', async () => {
      initVpn(/*doNotProvidePrefs=*/ true);
      await flushTasks();
    });

    test('OpenVPN does not show public key field', async () => {
      initVpn();
      await flushTasks();
      assertNull(
          internetDetailPage.shadowRoot!.querySelector('#wgPublicKeyField'));
    });

    test('WireGuard does show public key field', async () => {
      initWireGuard();
      await flushTasks();
      assertTrue(
          !!internetDetailPage.shadowRoot!.querySelector('#wgPublicKeyField'));
    });

    test('Advanced section hidden when properties are not set', async () => {
      initVpnWithNoAdvancedProperties();
      await flushTasks();
      const expandButtons =
          internetDetailPage.shadowRoot!.querySelectorAll<HTMLButtonElement>(
              'cr-expand-button.settings-box');
      expandButtons.forEach((button: HTMLButtonElement) => {
        assertNotEquals('Advanced', button.textContent!.trim());
      });
    });
  });

  suite('DetailsPageCellular', () => {
    async function expandConfigurableSection(): Promise<void> {
      const configurableSections =
          internetDetailPage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#configurableSections');
      assertTrue(!!configurableSections);
      configurableSections.click();
      await flushTasks();
      assertTrue(internetDetailPage.get('showConfigurableSections_'));
    }

    // Regression test for https://crbug.com/1182884.
    test('Connect button enabled when not connectable', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      mojoApi.setManagedPropertiesForTest(cellularNetwork);

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      await flushTasks();
      const connectButton = getButton('connectDisconnect');
      assertFalse(connectButton.hidden);
      assertFalse(connectButton.disabled);
    });

    test('carrier locked subtext when carrier locked', async () => {
      const TEST_ICCID = '11111111111111111';
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      mojoApi.setDeviceStateForTest({
        ...getDefaultDeviceStateProps(),
        deviceState: DeviceStateType.kEnabled,
        simLockStatus: {
          lockEnabled: true,
          lockType: 'network-pin',
          retriesLeft: 0,
        },
        simInfos: [{
          iccid: TEST_ICCID,
          isPrimary: true,
          slotId: 0,
          eid: '',
        }],
      });

      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
      cellularNetwork.typeProperties.cellular!.simLocked = true;
      mojoApi.setManagedPropertiesForTest(cellularNetwork);

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      await flushTasks();
      const carrierLockedText =
          internetDetailPage.shadowRoot!.querySelector<LocalizedLinkElement>(
              '#carrierLockedNoticeLink');
      assertTrue(!!carrierLockedText);
      assertEquals(
          internetDetailPage.i18nAdvanced('networkCarrierLocked').toString(),
          carrierLockedText.localizedString.toString());

      // Verify network state
      const networkStateText =
          internetDetailPage.shadowRoot!.querySelector('#networkState');
      assertTrue(!!networkStateText);
      assertTrue(networkStateText.hasAttribute('warning'));
      assertEquals(
          internetDetailPage.i18n('networkMobileProviderLocked'),
          networkStateText.textContent!.trim());
    });

    test(
        'Connect button disabled when not connectable and locked', async () => {
          init();
          mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
          const cellularNetwork =
              getManagedProperties(NetworkType.kCellular, 'cellular');
          cellularNetwork.connectable = false;
          cellularNetwork.typeProperties.cellular!.simLocked = true;
          mojoApi.setManagedPropertiesForTest(cellularNetwork);

          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          await flushTasks();
          const connectButton = getButton('connectDisconnect');
          assertFalse(connectButton.hidden);
          assertTrue(connectButton.disabled);
        });

    test(
        'Cellular view account button opens carrier account details',
        async () => {
          init();
          mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
          const cellularNetwork =
              getManagedProperties(NetworkType.kCellular, 'cellular');
          mojoApi.setManagedPropertiesForTest(cellularNetwork);

          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          await flushTasks();
          const viewAccountButton =
              internetDetailPage.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#viewAccountButton');
          assertTrue(!!viewAccountButton);
          viewAccountButton.click();
          await flushTasks();
          await browserProxy.whenCalled('showCarrierAccountDetail');
        });

    test(
        'Unactivated eSIM does not show activate or view account button',
        async () => {
          init();
          mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
          const cellularNetwork =
              getManagedProperties(NetworkType.kCellular, 'cellular');
          cellularNetwork.typeProperties.cellular!.eid = 'eid';
          cellularNetwork.connectionState = ConnectionStateType.kConnected;
          cellularNetwork.typeProperties.cellular!.activationState =
              ActivationStateType.kNotActivated;
          cellularNetwork.typeProperties.cellular!.paymentPortal = {
            url: 'url',
            method: '',
            postData: undefined,
          };
          mojoApi.setManagedPropertiesForTest(cellularNetwork);

          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          await flushTasks();
          const activateButton =
              internetDetailPage.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#activateButton');
          assertTrue(!!activateButton);
          assertTrue(activateButton.hidden);

          const viewAccountButton =
              internetDetailPage.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#viewAccountButton');
          assertTrue(!!viewAccountButton);
          assertTrue(viewAccountButton.hidden);
        });

    test('Cellular Scanning', async () => {
      const TEST_ICCID = '11111111111111111';

      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
      mojoApi.setManagedPropertiesForTest(cellularNetwork);

      mojoApi.setDeviceStateForTest({
        ...getDefaultDeviceStateProps(),
        deviceState: DeviceStateType.kEnabled,
        scanning: true,
        simInfos: [{
          iccid: TEST_ICCID,
          isPrimary: true,
          slotId: 0,
          eid: '',
        }],
      });

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      await flushTasks();
      const spinner =
          internetDetailPage.shadowRoot!.querySelector('paper-spinner-lite');
      assertTrue(!!spinner);
      assertFalse(spinner.hidden);
    });

    // Regression test for https://crbug.com/1201449.
    test('Page closed while device is updating', async () => {
      init();

      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      mojoApi.setManagedPropertiesForTest(cellularNetwork);

      mojoApi.setDeviceStateForTest({
        ...getDefaultDeviceStateProps(),
        deviceState: DeviceStateType.kEnabled,
        scanning: true,
      });

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');

      await flushTasks();
      // Close the page as soon as getDeviceStateList() is invoked, before the
      // callback returns.
      mojoApi.beforeGetDeviceStateList = () => {
        internetDetailPage.close();
      };
      mojoApi.onDeviceStateListChanged();
      await flushTasks();
    });

    test('Deep link to disconnect button', async () => {
      // Add listener for popstate event fired when the dialog closes and the
      // router navigates backwards.
      const popStatePromise = eventToPromise('popstate', window);

      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      mojoApi.setManagedPropertiesForTest(cellularNetwork);

      const DISCONNECT_CELLULAR_NETWORK_SETTING =
          settingMojom.Setting.kDisconnectCellularNetwork.toString();
      const params = new URLSearchParams();
      params.append('guid', 'cellular_guid');
      params.append('type', 'Cellular');
      params.append('name', 'cellular');
      params.append('settingId', DISCONNECT_CELLULAR_NETWORK_SETTING);
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await flushTasks();

      const deepLinkElement =
          getButton('connectDisconnect').shadowRoot!.querySelector('cr-button');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Disconnect network button should be focused for settingId=${
              DISCONNECT_CELLULAR_NETWORK_SETTING}.`);

      // Close the dialog and wait for os_route's popstate listener to fire. If
      // we don't add this wait, this event can fire during the next test which
      // will interfere with its routing.
      internetDetailPage.close();
      await popStatePromise;
    });

    test('Deep link to cellular roaming toggle button', async () => {
      const TEST_ICCID = '11111111111111111';

      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
      cellularNetwork.connectable = false;
      // Required for allowDataRoamingButton to be rendered.
      cellularNetwork.typeProperties.cellular!.allowRoaming =
          OncMojo.createManagedBool(false);
      mojoApi.setManagedPropertiesForTest(cellularNetwork);

      // Set SIM as active so that configurable sections are displayed.
      mojoApi.setDeviceStateForTest({
        ...getDefaultDeviceStateProps(),
        deviceState: DeviceStateType.kEnabled,
        simInfos: [{
          iccid: TEST_ICCID,
          isPrimary: true,
          slotId: 0,
          eid: '',
        }],
      });

      const CELLULAR_ROAMING_SETTING =
          settingMojom.Setting.kCellularRoaming.toString();
      const params = new URLSearchParams();
      params.append('guid', 'cellular_guid');
      params.append('type', 'Cellular');
      params.append('name', 'cellular');
      params.append('settingId', CELLULAR_ROAMING_SETTING);
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await flushTasks();

      // Attempting to focus a <network-config-toggle> will result in the focus
      // being pushed onto the internal <cr-toggle>.
      const toggleBtn = internetDetailPage.shadowRoot!
                            .querySelector<CellularRoamingToggleButtonElement>(
                                'cellular-roaming-toggle-button');
      assertTrue(!!toggleBtn);
      const cellularRoamingToggle = toggleBtn.getCellularRoamingToggle();
      assertTrue(!!cellularRoamingToggle);
      const deepLinkElement =
          cellularRoamingToggle.shadowRoot!.querySelector('cr-toggle');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Cellular roaming toggle button should be focused for settingId=${
              CELLULAR_ROAMING_SETTING}.`);
    });

    test('Deep link to sim lock toggle', async () => {
      await deepLinkToSimLockElement(/*isSimLocked=*/ false);

      const simInfo = internetDetailPage.shadowRoot!.querySelector<HTMLElement>(
          '#cellularSimInfoAdvanced');
      assertTrue(!!simInfo);

      // In this rare case, wait after next render twice due to focus behavior
      // of the siminfo component.
      await waitAfterNextRender(simInfo);
      await waitAfterNextRender(simInfo);
      assertEquals(
          simInfo.shadowRoot!.querySelector('#simLockButton'),
          getDeepActiveElement(),
          `Sim lock toggle should be focused for settingId=${
              CELLULAR_SIM_LOCK_SETTING}.`);
    });

    test('Deep link to sim unlock button', async () => {
      await deepLinkToSimLockElement(/*isSimLocked=*/ true);

      const simInfo = internetDetailPage.shadowRoot!.querySelector<HTMLElement>(
          '#cellularSimInfoAdvanced');
      assertTrue(!!simInfo);

      // In this rare case, wait after next render twice due to focus behavior
      // of the siminfo component.
      await waitAfterNextRender(simInfo);
      await waitAfterNextRender(simInfo);
      assertEquals(
          simInfo.shadowRoot!.querySelector('#unlockPinButton'),
          getDeepActiveElement(),
          `Sim unlock button should be focused for settingId=${
              CELLULAR_SIM_LOCK_SETTING}.`);
    });

    test('Cellular page hides hidden toggle', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = false;
      mojoApi.setManagedPropertiesForTest(cellularNetwork);

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      await flushTasks();
      const hiddenToggle = getHiddenToggle();
      assertNull(hiddenToggle);
    });

    test(
        'Cellular network on active sim slot, show config sections',
        async () => {
          loadTimeData.overrideValues({isApnRevampEnabled: true});
          init();
          const TEST_ICCID = '11111111111111111';

          await mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
          const cellularNetwork = getManagedProperties(
              NetworkType.kCellular, 'cellular', OncSource.kDevice);
          cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
          // Required for allowDataRoamingButton to be rendered.
          cellularNetwork.typeProperties.cellular!.allowRoaming =
              OncMojo.createManagedBool(false);

          mojoApi.setManagedPropertiesForTest(cellularNetwork);
          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          mojoApi.setDeviceStateForTest({
            ...getDefaultDeviceStateProps(),
            deviceState: DeviceStateType.kEnabled,
            simInfos: [{
              iccid: TEST_ICCID,
              isPrimary: true,
              slotId: 0,
              eid: '',
            }],
          });
          await flushTasks();
          assertTrue(internetDetailPage.get('showConfigurableSections_'));
          // Check that an element from the primary account section exists.
          const toggleBtn =
              internetDetailPage.shadowRoot!
                  .querySelector<CellularRoamingToggleButtonElement>(
                      'cellular-roaming-toggle-button');
          assertTrue(!!toggleBtn);
          assertTrue(!!toggleBtn.getCellularRoamingToggle());
          assertTrue(!!internetDetailPage.shadowRoot!.querySelector(
              '#apnSubpageButton'));
        });

    test(
        'Cellular network on non-active sim slot, hide config sections',
        async () => {
          loadTimeData.overrideValues({isApnRevampEnabled: true});
          init();
          const TEST_ICCID = '11111111111111111';

          await mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
          const cellularNetwork = getManagedProperties(
              NetworkType.kCellular, 'cellular', OncSource.kDevice);
          cellularNetwork.typeProperties.cellular!.iccid = '000';

          mojoApi.setManagedPropertiesForTest(cellularNetwork);
          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          mojoApi.setDeviceStateForTest({
            ...getDefaultDeviceStateProps(),
            deviceState: DeviceStateType.kEnabled,
            simInfos: [{
              iccid: TEST_ICCID,
              isPrimary: true,
              slotId: 0,
              eid: '',
            }],
          });
          await flushTasks();
          assertFalse(internetDetailPage.get('showConfigurableSections_'));
          // Check that an element from the primary account section exists.
          assertNull(internetDetailPage.shadowRoot!.querySelector(
              '#allowDataRoaming'));
          // The section ConnectDisconnect button belongs to should still be
          // showing.
          assertTrue(!!internetDetailPage.shadowRoot!.querySelector(
              '#connectDisconnect'));
          assertNull(internetDetailPage.shadowRoot!.querySelector(
              '#apnSubpageButton'));
        });

    test(
        'Hide config section and Cellular Device object fields when' +
            'sim becomes non-active',
        async () => {
          init();
          const TEST_ICCID = '11111111111111111';

          await mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
          const cellularNetwork = getManagedProperties(
              NetworkType.kCellular, 'cellular', OncSource.kDevice);
          cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;

          const isShowingCellularDeviceObjectFields = () => {
            const deviceFields =
                internetDetailPage.shadowRoot!
                    .querySelector<NetworkPropertyListMojoElement>(
                        '#deviceFields');
            assertTrue(!!deviceFields);
            return deviceFields.fields.includes('cellular.homeProvider.name');
          };

          // Set sim to non-active.
          mojoApi.setManagedPropertiesForTest(cellularNetwork);
          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          mojoApi.setDeviceStateForTest({
            ...getDefaultDeviceStateProps(),
            deviceState: DeviceStateType.kEnabled,
            simInfos: [{
              iccid: TEST_ICCID,
              isPrimary: false,
              slotId: 0,
              eid: '',
            }],
          });
          await flushTasks();
          assertFalse(internetDetailPage.get('showConfigurableSections_'));
          assertFalse(isShowingCellularDeviceObjectFields());

          // Set sim to active.
          mojoApi.setDeviceStateForTest({
            ...getDefaultDeviceStateProps(),
            deviceState: DeviceStateType.kEnabled,
            simInfos: [{
              iccid: TEST_ICCID,
              isPrimary: true,
              slotId: 0,
              eid: '',
            }],
          });
          await flushTasks();
          assertTrue(internetDetailPage.get('showConfigurableSections_'));
          assertTrue(isShowingCellularDeviceObjectFields());

          // Set sim to non-active again.
          mojoApi.setDeviceStateForTest({
            ...getDefaultDeviceStateProps(),
            deviceState: DeviceStateType.kEnabled,
            simInfos: [{
              iccid: TEST_ICCID,
              isPrimary: false,
              slotId: 0,
              eid: '',
            }],
          });
          await flushTasks();
          assertFalse(internetDetailPage.get('showConfigurableSections_'));
          assertFalse(isShowingCellularDeviceObjectFields());
        });

    test('Do not show MAC address', async () => {
      const TEST_ICCID = '11111111111111111';
      const TEST_MAC_ADDRESS = '01:23:45:67:89:AB';
      const MISSING_MAC_ADDRESS = '00:00:00:00:00:00';

      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.connectable = true;
      cellularNetwork.typeProperties.cellular!.simLocked = false;
      cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
      mojoApi.setManagedPropertiesForTest(cellularNetwork);
      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');

      let deviceState = {
        ...getDefaultDeviceStateProps(),
        deviceState: DeviceStateType.kEnabled,
        simInfos: [{
          iccid: TEST_ICCID,
          isPrimary: true,
          slotId: 0,
          eid: '',
        }],
        macAddress: TEST_MAC_ADDRESS,
      };

      mojoApi.setDeviceStateForTest(deviceState);
      await flushTasks();
      expandConfigurableSection();
      let macAddress =
          internetDetailPage.shadowRoot!.querySelector<HTMLElement>(
              '#mac-address-container');
      assertTrue(!!macAddress);
      assertFalse(macAddress.hidden);

      // Set MAC address to '00:00:00:00:00:00' missing address, this address
      // is provided when device MAC address cannot be retrieved. If this is the
      // case, the MAC address should not be displayed in UI.
      deviceState = {
        ...getDefaultDeviceStateProps(),
        deviceState: DeviceStateType.kEnabled,
        simInfos: [{
          iccid: TEST_ICCID,
          isPrimary: true,
          slotId: 0,
          eid: '',
        }],
        macAddress: MISSING_MAC_ADDRESS,
      };
      mojoApi.setDeviceStateForTest(deviceState);
      await flushTasks();
      expandConfigurableSection();
      macAddress = internetDetailPage.shadowRoot!.querySelector<HTMLElement>(
          '#mac-address-container');
      assertTrue(!!macAddress);
      assertTrue(macAddress.hidden);
    });

    // Syntactic sugar for running test twice with different values for the
    // apnRevamp feature flag.
    [true, false].forEach(isApnRevampEnabled => {
      test(
          `Page disabled when inhibited, ApnRevamp enabled is: ${
              isApnRevampEnabled}`,
          async () => {
            loadTimeData.overrideValues({isApnRevampEnabled});
            init();

            mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
            const cellularNetwork = getManagedProperties(
                NetworkType.kCellular, 'cellular', OncSource.kDevice);
            // Required for connectDisconnectButton to be rendered.
            cellularNetwork.connectionState = ConnectionStateType.kConnected;
            // Required for allowDataRoamingButton to be rendered.
            cellularNetwork.typeProperties.cellular!.allowRoaming =
                OncMojo.createManagedBool(false);
            // Required for advancedFields to be rendered.
            cellularNetwork.typeProperties.cellular!.networkTechnology = 'LTE';
            // Required for infoFields to be rendered.
            cellularNetwork.typeProperties.cellular!.servingOperator = {
              name: 'name',
              code: '',
              country: '',
            };
            // Required for deviceFields to be rendered.
            const TEST_ICCID = '11111111111111111';
            cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
            // Required for networkChooseMobile to be rendered.
            cellularNetwork.typeProperties.cellular!.supportNetworkScan = true;
            mojoApi.setManagedPropertiesForTest(cellularNetwork);

            // Start uninhibited.
            mojoApi.setDeviceStateForTest({
              ...getDefaultDeviceStateProps(),
              deviceState: DeviceStateType.kEnabled,
              // Required for configurable sections to be rendered.
              simInfos: [{
                iccid: TEST_ICCID,
                isPrimary: true,
                slotId: 0,
                eid: '',
              }],
            });

            internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');

            await flushTasks();

            const connectDisconnectButton = getButton('connectDisconnect');
            const infoFields = getButton('infoFields');
            const cellularSimInfoAdvanced =
                getButton('cellularSimInfoAdvanced');
            const advancedFields = getButton('advancedFields');
            const deviceFields = getButton('deviceFields');
            const toggleBtn =
                internetDetailPage.shadowRoot!
                    .querySelector<CellularRoamingToggleButtonElement>(
                        'cellular-roaming-toggle-button');
            assertTrue(!!toggleBtn);
            const allowDataRoamingButton = toggleBtn.getCellularRoamingToggle();
            const networkChooseMobile =
                internetDetailPage.shadowRoot!
                    .querySelector<NetworkChooseMobileElement>(
                        'network-choose-mobile');
            let apnElement: CrLinkRowElement|NetworkApnListElement|null =
                internetDetailPage.shadowRoot!.querySelector<CrLinkRowElement>(
                    '#apnSubpageButton');
            if (!isApnRevampEnabled) {
              apnElement =
                  internetDetailPage.shadowRoot!
                      .querySelector<NetworkApnListElement>('network-apnlist');
            }
            const networkIpConfig =
                internetDetailPage.shadowRoot!
                    .querySelector<NetworkIpConfigElement>('network-ip-config');
            const networkNameservers =
                internetDetailPage.shadowRoot!
                    .querySelector<NetworkNameserversElement>(
                        'network-nameservers');
            const networkProxySection =
                internetDetailPage.shadowRoot!
                    .querySelector<NetworkProxySectionElement>(
                        'network-proxy-section');

            assertTrue(!!allowDataRoamingButton);
            assertTrue(!!networkChooseMobile);
            assertTrue(!!apnElement);
            assertTrue(!!networkIpConfig);
            assertTrue(!!networkNameservers);
            assertTrue(!!networkProxySection);

            assertFalse(connectDisconnectButton.disabled);
            assertFalse(allowDataRoamingButton.disabled);
            assertFalse(infoFields.disabled);
            assertFalse(cellularSimInfoAdvanced.disabled);
            assertFalse(advancedFields.disabled);
            assertFalse(deviceFields.disabled);
            assertFalse(networkChooseMobile.disabled);
            assertFalse(apnElement.disabled);
            assertFalse(networkIpConfig.disabled);
            assertFalse(networkNameservers.disabled);
            assertFalse(networkProxySection.disabled);

            // Mock device being inhibited.
            mojoApi.setDeviceStateForTest({
              ...getDefaultDeviceStateProps(),
              deviceState: DeviceStateType.kEnabled,
              inhibitReason: InhibitReason.kConnectingToProfile,
              simInfos: [{
                iccid: TEST_ICCID,
                isPrimary: true,
                slotId: 0,
                eid: '',
              }],
            });
            await flushTasks();

            assertTrue(connectDisconnectButton.disabled);
            assertTrue(allowDataRoamingButton.disabled);
            assertTrue(infoFields.disabled);
            assertTrue(cellularSimInfoAdvanced.disabled);
            assertTrue(advancedFields.disabled);
            assertTrue(deviceFields.disabled);
            assertTrue(networkChooseMobile.disabled);
            assertTrue(apnElement.disabled);
            assertTrue(networkIpConfig.disabled);
            assertTrue(networkNameservers.disabled);
            assertTrue(networkProxySection.disabled);

            // Uninhibit.
            mojoApi.setDeviceStateForTest({
              ...getDefaultDeviceStateProps(),
              deviceState: DeviceStateType.kEnabled,
              simInfos: [{
                iccid: TEST_ICCID,
                isPrimary: true,
                slotId: 0,
                eid: '',
              }],
            });
            await flushTasks();

            assertFalse(connectDisconnectButton.disabled);
            assertFalse(allowDataRoamingButton.disabled);
            assertFalse(infoFields.disabled);
            assertFalse(cellularSimInfoAdvanced.disabled);
            assertFalse(advancedFields.disabled);
            assertFalse(deviceFields.disabled);
            assertFalse(networkChooseMobile.disabled);
            assertFalse(apnElement.disabled);
            assertFalse(networkIpConfig.disabled);
            assertFalse(networkNameservers.disabled);
            assertFalse(networkProxySection.disabled);
          });
    });

    test('Cellular page disabled when blocked by policy', async () => {
      init();

      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
      const cellularNetwork = getManagedProperties(
          NetworkType.kCellular, 'cellular', OncSource.kDevice);
      // Required for connectDisconnectButton to be rendered.
      cellularNetwork.connectionState = ConnectionStateType.kNotConnected;
      cellularNetwork.typeProperties.cellular!.allowRoaming =
          OncMojo.createManagedBool(false);
      // Required for advancedFields to be rendered.
      cellularNetwork.typeProperties.cellular!.networkTechnology = 'LTE';
      // Required for infoFields to be rendered.
      cellularNetwork.typeProperties.cellular!.servingOperator = {
        name: 'name',
        code: '',
        country: '',
      };
      // Required for deviceFields to be rendered.
      const TEST_ICCID = '11111111111111111';
      cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
      cellularNetwork.typeProperties.cellular!.supportNetworkScan = true;
      cellularNetwork.source = OncSource.kNone;
      mojoApi.setManagedPropertiesForTest(cellularNetwork);

      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      internetDetailPage.globalPolicy = {
        ...getDefaultGlobalPolicy(),
        allowOnlyPolicyCellularNetworks: true,
      };
      await flushTasks();

      const connectDisconnectButton = getButton('connectDisconnect');
      assertTrue(connectDisconnectButton.hidden);
      assertTrue(connectDisconnectButton.disabled);
      assertNull(internetDetailPage.shadowRoot!.querySelector('#infoFields'));
      const cellularSimInfoAdvanced = getButton('cellularSimInfoAdvanced');
      assertFalse(cellularSimInfoAdvanced.disabled);
      assertFalse(cellularSimInfoAdvanced.hidden);
      const advancedFields = getButton('advancedFields');
      assertFalse(advancedFields.disabled);
      assertFalse(advancedFields.hidden);
      const deviceFields = getButton('deviceFields');
      assertFalse(deviceFields.disabled);
      assertFalse(deviceFields.hidden);

      assertNull(internetDetailPage.shadowRoot!.querySelector(
          'cellular-roaming-toggle-button'));
      assertNull(internetDetailPage.shadowRoot!.querySelector(
          'network-choose-mobile'));
      assertNull(
          internetDetailPage.shadowRoot!.querySelector('network-apnlist'));
      assertNull(
          internetDetailPage.shadowRoot!.querySelector('network-ip-config'));
      assertNull(
          internetDetailPage.shadowRoot!.querySelector('network-nameservers'));
      assertNull(internetDetailPage.shadowRoot!.querySelector(
          'network-proxy-section'));
    });

    // Syntactic sugar for running test twice with different values for the
    // apnRevamp feature flag.
    [true, false].forEach(isApnRevampEnabled => {
      test('Show/Hide APN row correspondingly to ApnRevamp flag', async () => {
        loadTimeData.overrideValues({isApnRevampEnabled});
        init();
        mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
        const apnName = 'test';
        const testIccid = '11111';
        const cellularNetwork =
            getManagedProperties(NetworkType.kCellular, 'cellular');
        cellularNetwork.typeProperties.cellular!.connectedApn = {
          accessPointName: '',
          id: '',
          authentication: ApnAuthenticationType.kAutomatic,
          language: undefined,
          localizedName: undefined,
          name: undefined,
          password: undefined,
          username: undefined,
          attach: undefined,
          state: ApnState.kEnabled,
          ipType: ApnIpType.kAutomatic,
          apnTypes: [],
          source: ApnSource.kModb,
        };
        cellularNetwork.typeProperties.cellular!.connectedApn!.accessPointName =
            apnName;
        cellularNetwork.typeProperties.cellular!.iccid = testIccid;
        mojoApi.setManagedPropertiesForTest(cellularNetwork);
        internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');

        // Set cellular network as active SIM so that APN row should show up if
        // the flag is enabled.
        mojoApi.setDeviceStateForTest({
          ...getDefaultDeviceStateProps(),
          deviceState: DeviceStateType.kEnabled,
          simInfos: [{
            iccid: testIccid,
            isPrimary: true,
            slotId: 0,
            eid: '',
          }],
        });
        await flushTasks();
        const getCrLink = () =>
            internetDetailPage.shadowRoot!.querySelector('#apnSubpageButton');
        const getApn = () => getCrLink() ?
            getCrLink()!.shadowRoot!.querySelector('#subLabel') :
            null;
        if (isApnRevampEnabled) {
          assertTrue(!!getApn());
          assertEquals(apnName, getApn()!.textContent!.trim());

          const name = 'name';
          cellularNetwork.typeProperties.cellular!.connectedApn.name = name;
          mojoApi.setManagedPropertiesForTest(cellularNetwork);
          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          await flushTasks();
          assertTrue(!!getApn());
          assertEquals(name, getApn()!.textContent!.trim());
          assertFalse(getCrLink()!.hasAttribute('warning'));

          // Adding a restricted connectivity state should cause the sublabel to
          // be a warning.
          cellularNetwork.portalState = PortalState.kNoInternet;
          cellularNetwork.connectionState = ConnectionStateType.kPortal;
          mojoApi.setManagedPropertiesForTest(cellularNetwork);
          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          await flushTasks();
          assertTrue(getCrLink()!.hasAttribute('warning'));
        } else {
          assertNull(getApn());
        }
      });
    });

    [true, false].forEach(isApnRevampAndAllowApnModificationPolicyEnabled => {
      test(
          `Managed APN icon visibility when ` +
              `isApnRevampAndAllowApnModificationPolicyEnabled is ${
                  isApnRevampAndAllowApnModificationPolicyEnabled}`,
          async () => {
            loadTimeData.overrideValues({
              isApnRevampEnabled: true,
              isApnRevampAndAllowApnModificationPolicyEnabled:
                  isApnRevampAndAllowApnModificationPolicyEnabled,
            });
            init();
            mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
            const apnName = 'test';
            const testIccid = '11111';
            const cellularNetwork =
                getManagedProperties(NetworkType.kCellular, 'cellular');
            cellularNetwork.typeProperties.cellular!.connectedApn = {
              accessPointName: '',
              id: '',
              authentication: ApnAuthenticationType.kAutomatic,
              language: undefined,
              localizedName: undefined,
              name: undefined,
              password: undefined,
              username: undefined,
              attach: undefined,
              state: ApnState.kEnabled,
              ipType: ApnIpType.kAutomatic,
              apnTypes: [],
              source: ApnSource.kModb,
            };
            cellularNetwork.typeProperties.cellular!.connectedApn!
                .accessPointName = apnName;
            cellularNetwork.typeProperties.cellular!.iccid = testIccid;
            mojoApi.setManagedPropertiesForTest(cellularNetwork);
            internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');

            // Set cellular network as active SIM so that APN row should show up
            // if the flag is enabled.
            mojoApi.setDeviceStateForTest({
              ...getDefaultDeviceStateProps(),
              deviceState: DeviceStateType.kEnabled,
              simInfos: [{
                iccid: testIccid,
                isPrimary: true,
                slotId: 0,
                eid: '',
              }],
            });
            await flushTasks();
            assertTrue(!!internetDetailPage.shadowRoot!.querySelector(
                '#apnSubpageButton'));

            // Check for APN policies managed icon.
            const getApnManagedIcon = () =>
                internetDetailPage.shadowRoot!.querySelector('#apnManagedIcon');
            assertFalse(!!getApnManagedIcon());

            internetDetailPage.globalPolicy = {
              ...getDefaultGlobalPolicy(),
              allowApnModification: true,
            };
            await flushTasks();
            assertFalse(!!getApnManagedIcon());

            internetDetailPage.globalPolicy = {
              ...getDefaultGlobalPolicy(),
              allowApnModification: false,
            };
            await flushTasks();
            assertEquals(
                isApnRevampAndAllowApnModificationPolicyEnabled,
                !!getApnManagedIcon());
          });
    });

    test('Cellular network not found while in detail subpage', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);

      // Simulate navigating to mobile data subpage.
      let params = new URLSearchParams();
      params.append(
          'type', OncMojo.getNetworkTypeString(NetworkType.kCellular));
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
      assertEquals(routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);
      await flushTasks();

      // Navigate to cellular detail page. Because the network is not found, the
      // page should navigate backwards.
      const popStatePromise = eventToPromise('popstate', window);
      params = new URLSearchParams();
      params.append('guid', 'cellular_guid');
      params.append('type', 'Cellular');
      params.append('name', 'cellular');
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await popStatePromise;
      assertEquals(routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);
    });

    // Regression test for b/281728200.
    test('Cellular network not found while not in detail subpage', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);

      // Simulate navigating to top-level internet page.
      let params = new URLSearchParams();
      Router.getInstance().navigateTo(routes.INTERNET, params);
      assertEquals(routes.INTERNET, Router.getInstance().currentRoute);

      // Simulate navigating to mobile data subpage.
      params = new URLSearchParams();
      params.append(
          'type', OncMojo.getNetworkTypeString(NetworkType.kCellular));
      Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
      assertEquals(routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);
      await flushTasks();

      // Trigger |internetDetailPage| attempting to fetch the network. Because
      // the page is not the current route, it should not trigger a navigation
      // backwards.
      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      await flushTasks();

      assertEquals(routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);
    });

      test(
          'Show/Hide toggle correspondingly to SuppressTextMessages flag',
          async () => {
            init();
            mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);
            const TEST_ICCID = '11111111111111111';
            const cellularNetwork =
                getManagedProperties(NetworkType.kCellular, 'cellular');
            cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
            mojoApi.setManagedPropertiesForTest(cellularNetwork);
            internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
            mojoApi.setDeviceStateForTest({
              ...getDefaultDeviceStateProps(),
              deviceState: DeviceStateType.kEnabled,
              simInfos: [{
                iccid: TEST_ICCID,
                isPrimary: true,
                slotId: 0,
                eid: '',
              }],
            });
            await flushTasks();
            const getSuppressTextMessagesToggle = () =>
                internetDetailPage.shadowRoot!.querySelector(
                    '#suppressTextMessagesToggle');
              assertTrue(!!getSuppressTextMessagesToggle());

          });

    test('Suppress text messages manually', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);

      const TEST_ICCID = '11111111111111111';
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
      cellularNetwork.typeProperties.cellular!.allowTextMessages = {
        activeValue: true,
        policySource: PolicySource.kNone,
        policyValue: false,
      };
      mojoApi.setManagedPropertiesForTest(cellularNetwork);
      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      mojoApi.setDeviceStateForTest({
        ...getDefaultDeviceStateProps(),
        deviceState: DeviceStateType.kEnabled,
        simInfos: [{
          iccid: TEST_ICCID,
          isPrimary: true,
          slotId: 0,
          eid: '',
        }],
      });
      await flushTasks();

      const networkToggle = internetDetailPage.shadowRoot!
                                .querySelector<NetworkConfigToggleElement>(
                                    '#suppressTextMessagesToggle');
      assertTrue(!!networkToggle);
      assertTrue(networkToggle.checked);
      networkToggle.click();
      await mojoApi.whenCalled('setProperties');
      const props = mojoApi.getPropertiesToSetForTest();
      assertTrue(!!props);
      assertFalse(
          props.typeConfig.cellular!.textMessageAllowState!.allowTextMessages);
    });

    test(
        'Suppress text messages toggle only shown for active SIM', async () => {
          init();
          mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);

          const TEST_ICCID = '11111111111111111';
          const cellularNetwork =
              getManagedProperties(NetworkType.kCellular, 'cellular');
          cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
          mojoApi.setManagedPropertiesForTest(cellularNetwork);
          internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
          await flushTasks();
          const getSuppressTextMessagesToggle = () =>
              internetDetailPage.shadowRoot!.querySelector(
                  '#suppressTextMessagesToggle');
          assertNull(getSuppressTextMessagesToggle());
          mojoApi.setDeviceStateForTest({
            ...getDefaultDeviceStateProps(),
            deviceState: DeviceStateType.kEnabled,
            simInfos: [{
              iccid: TEST_ICCID,
              isPrimary: true,
              slotId: 0,
              eid: '',
            }],
          });
          await flushTasks();
          assertTrue(!!getSuppressTextMessagesToggle());
        });

    test('Suppress text messages via policy', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kCellular, true);

      const TEST_ICCID = '11111111111111111';
      const cellularNetwork =
          getManagedProperties(NetworkType.kCellular, 'cellular');
      cellularNetwork.typeProperties.cellular!.allowTextMessages = {
        activeValue: true,
        policySource: PolicySource.kDevicePolicyEnforced,
        policyValue: false,
      };
      cellularNetwork.typeProperties.cellular!.iccid = TEST_ICCID;
      mojoApi.setManagedPropertiesForTest(cellularNetwork);
      internetDetailPage.init('cellular_guid', 'Cellular', 'cellular');
      mojoApi.setDeviceStateForTest({
        ...getDefaultDeviceStateProps(),
        deviceState: DeviceStateType.kEnabled,
        simInfos: [{
          iccid: TEST_ICCID,
          isPrimary: true,
          slotId: 0,
          eid: '',
        }],
      });
      await flushTasks();

      const networkToggle = internetDetailPage.shadowRoot!
                                .querySelector<NetworkConfigToggleElement>(
                                    '#suppressTextMessagesToggle');
      assertTrue(!!networkToggle);
      assertTrue(networkToggle.checked);
      const crToggle =
          networkToggle.shadowRoot!.querySelector<HTMLButtonElement>(
              'cr-toggle');
      assertTrue(!!crToggle);
      assertTrue(crToggle.disabled);
      assertTrue(!!networkToggle.shadowRoot!.querySelector(
          'cr-policy-network-indicator-mojo'));
    });
  });

  suite('DetailsPageEthernet', () => {
    test('LoadPage', () => {
      init();
    });

    test('Eth1', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kEthernet, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kEthernet, 'eth1'),
      ]);

      internetDetailPage.init('eth1_guid', 'Ethernet', 'eth1');
      assertEquals('eth1_guid', internetDetailPage.guid);
      await flushTasks();
      await mojoApi.whenCalled('getManagedProperties');
    });

    test('Deep link to configure ethernet button', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kEthernet, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kEthernet, 'eth1'),
      ]);

      const CONFIGURE_ETHERNET_SETTING =
          settingMojom.Setting.kConfigureEthernet.toString();
      const params = new URLSearchParams();
      params.append('guid', 'eth1_guid');
      params.append('type', 'Ethernet');
      params.append('name', 'eth1');
      params.append('settingId', CONFIGURE_ETHERNET_SETTING);
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await flushTasks();

      const deepLinkElement = getButton('configureButton');
      await waitAfterNextRender(deepLinkElement);

      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Configure ethernet button should be focused for settingId=${
              CONFIGURE_ETHERNET_SETTING}.`);
    });
  });

  suite('DetailsPageTether', () => {
    test('LoadPage', () => {
      init();
    });

    test(
        'Create tether network, first connection attempt shows tether dialog',
        async () => {
          init();
          mojoApi.setNetworkTypeEnabledState(NetworkType.kTether, true);
          setNetworksForTest([
            OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
          ]);

          internetDetailPage.init('tether1_guid', 'Tether', 'tether1');
          assertEquals('tether1_guid', internetDetailPage.guid);
          await flushTasks();
          await mojoApi.whenCalled('getManagedProperties');

          const connect =
              internetDetailPage.shadowRoot!.querySelector<HTMLButtonElement>(
                  '#connectDisconnect');
          assertTrue(!!connect);

          const tetherDialog =
              internetDetailPage.shadowRoot!.querySelector('#tetherDialog');
          assertTrue(!!tetherDialog);
          let crDialog =
              tetherDialog.shadowRoot!.querySelector<CrDialogElement>(
                  '#dialog');
          assertTrue(!!crDialog);
          assertFalse(crDialog.open);

          let showTetherDialogFinished: (val: null) => void;
          const showTetherDialogPromise = new Promise((resolve) => {
            showTetherDialogFinished = resolve;
          });
          const showTetherDialog = internetDetailPage['showTetherDialog_'];
          internetDetailPage['showTetherDialog_'] = () => {
            showTetherDialog.call(internetDetailPage);
            showTetherDialogFinished(null);
          };

          connect.click();
          await showTetherDialogPromise;
          crDialog = tetherDialog.shadowRoot!.querySelector<CrDialogElement>(
              '#dialog');
          assertTrue(!!crDialog);
          assertTrue(crDialog.open);
        });

    test('Deep link to disconnect tether network', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kTether, true);
      setNetworksForTest([
        OncMojo.getDefaultNetworkState(NetworkType.kTether, 'tether1'),
      ]);
      const tetherNetwork =
          getManagedProperties(NetworkType.kTether, 'tether1');
      tetherNetwork.connectable = true;
      mojoApi.setManagedPropertiesForTest(tetherNetwork);

      await flushTasks();

      const DISCONNECT_TETHER_NETWORK_SETTING =
          settingMojom.Setting.kDisconnectTetherNetwork.toString();
      const params = new URLSearchParams();
      params.append('guid', 'tether1_guid');
      params.append('type', 'Tether');
      params.append('name', 'tether1');
      params.append('settingId', DISCONNECT_TETHER_NETWORK_SETTING);
      Router.getInstance().navigateTo(routes.NETWORK_DETAIL, params);

      await flushTasks();

      const deepLinkElement =
          getButton('connectDisconnect').shadowRoot!.querySelector('cr-button');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Disconnect tether button should be focused for settingId=${
              DISCONNECT_TETHER_NETWORK_SETTING}.`);
    });
  });

  suite('DetailsPageAutoConnect', () => {
    test('Auto Connect toggle updates after GUID change', async () => {
      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const wifi1 =
          getManagedProperties(NetworkType.kWiFi, 'wifi1', OncSource.kDevice);
      wifi1.typeProperties.wifi!.autoConnect = OncMojo.createManagedBool(true);
      mojoApi.setManagedPropertiesForTest(wifi1);

      const wifi2 =
          getManagedProperties(NetworkType.kWiFi, 'wifi2', OncSource.kDevice);
      wifi2.typeProperties.wifi!.autoConnect = OncMojo.createManagedBool(false);
      mojoApi.setManagedPropertiesForTest(wifi2);

      internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi1');
      await flushTasks();
      let toggle =
          internetDetailPage.shadowRoot!
              .querySelector<SettingsToggleButtonElement>('#autoConnectToggle');
      assertTrue(!!toggle);
      assertTrue(toggle.checked);
      internetDetailPage.init('wifi2_guid', 'WiFi', 'wifi2');
      await flushTasks();
      toggle =
          internetDetailPage.shadowRoot!.querySelector('#autoConnectToggle');
      assertTrue(!!toggle);
      assertFalse(toggle.checked);
    });

    test('Auto Connect updates don\'t trigger a re-save', async () => {
      const defaultNetworkStateProps: NetworkStateProperties = {
        guid: '',
        connectable: false,
        connectRequested: false,
        connectionState: ConnectionStateType.kOnline,
        errorState: undefined,
        name: '',
        portalState: PortalState.kUnknown,
        portalProbeUrl: undefined,
        priority: 0,
        proxyMode: ProxyMode.kDirect,
        prohibitedByPolicy: false,
        source: OncSource.kNone,
        type: NetworkType.kAll,
        typeState: {
          cellular: undefined,
          ethernet: undefined,
          tether: undefined,
          vpn: undefined,
          wifi: undefined,
        },
      };

      init();
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      let wifi1 =
          getManagedProperties(NetworkType.kWiFi, 'wifi1', OncSource.kDevice);
      wifi1.typeProperties.wifi!.autoConnect = OncMojo.createManagedBool(true);
      mojoApi.setManagedPropertiesForTest(wifi1);
      assertEquals(undefined, mojoApi.methodCalled('setProperties'));

      internetDetailPage.init('wifi1_guid', 'WiFi', 'wifi1');
      internetDetailPage.onNetworkStateChanged(
          {...defaultNetworkStateProps, guid: 'wifi1_guid'});
      await flushTasks();

      let toggle =
          internetDetailPage.shadowRoot!
              .querySelector<SettingsToggleButtonElement>('#autoConnectToggle');
      assertTrue(!!toggle);
      assertTrue(toggle.checked);
      // Rebuild the object to force polymer to recognize a change.
      wifi1 =
          getManagedProperties(NetworkType.kWiFi, 'wifi1', OncSource.kDevice);
      wifi1.typeProperties.wifi!.autoConnect = OncMojo.createManagedBool(false);
      mojoApi.setManagedPropertiesForTest(wifi1);

      internetDetailPage.onNetworkStateChanged(
          {...defaultNetworkStateProps, guid: 'wifi1_guid'});
      await flushTasks();
      toggle =
          internetDetailPage.shadowRoot!
              .querySelector<SettingsToggleButtonElement>('#autoConnectToggle');
      assertTrue(!!toggle);
      assertFalse(toggle.checked);
    });
  });
});
