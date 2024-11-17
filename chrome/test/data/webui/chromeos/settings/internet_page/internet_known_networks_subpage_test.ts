// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsInternetKnownNetworksPageElement} from 'chrome://os-settings/lazy_load.js';
import {CrActionMenuElement, CrIconButtonElement, Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakePasspointService} from 'chrome://webui-test/chromeos/fake_passpoint_service_mojom.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<settings-internet-known-networks-subpage>', () => {
  let internetKnownNetworksPage: SettingsInternetKnownNetworksPageElement;
  let mojoApi: FakeNetworkConfig;
  let passpointServiceApi: FakePasspointService;

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

    passpointServiceApi = new FakePasspointService();
    MojoConnectivityProvider.getInstance().setPasspointServiceForTest(
        passpointServiceApi);
  });

  function setNetworksForTest(networks: NetworkStateProperties[]): void {
    mojoApi.resetForTest();
    mojoApi.addNetworksForTest(networks);
  }

  teardown(() => {
    internetKnownNetworksPage.remove();
    mojoApi.resetForTest();
    passpointServiceApi.resetForTest();
    Router.getInstance().resetRouteForTesting();
  });

  async function init(): Promise<void> {
    internetKnownNetworksPage =
        document.createElement('settings-internet-known-networks-subpage');
    assertTrue(!!internetKnownNetworksPage);
    document.body.appendChild(internetKnownNetworksPage);
    await flushTasks();
  }

  suite('KnownNetworksPage', () => {
    test('WiFi', async () => {
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      setNetworksForTest([
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      const SETTING_ID_7 = settingMojom.Setting.kForgetWifiNetwork.toString();
      params.append('settingId', SETTING_ID_7);
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
      await flushTasks();

      assertEquals(
          2, internetKnownNetworksPage.get('networkStateList_').length);

      const preferredList = internetKnownNetworksPage.shadowRoot!.querySelector(
          '#preferredNetworkList');
      assertTrue(!!preferredList);
      const preferredElems = preferredList.querySelectorAll('cr-link-row');
      assertEquals(1, preferredElems.length);

      const deepLinkElement =
          preferredElems[0]!.shadowRoot!.querySelector<HTMLElement>('#icon');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Preferred list element should be focused for settingId=${
              SETTING_ID_7}.`);
    });

    test('Known networks policy icon and menu button a11y', async () => {
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      preferredWifi.source = OncSource.kDevicePolicy;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      notPreferredWifi.source = OncSource.kDevicePolicy;
      setNetworksForTest([
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append(
          'settingId', settingMojom.Setting.kForgetWifiNetwork.toString());
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
      await flushTasks();

      assertEquals(
          2, internetKnownNetworksPage.get('networkStateList_').length);
      const preferredList = internetKnownNetworksPage.shadowRoot!.querySelector(
          '#preferredNetworkList');
      assertTrue(!!preferredList);

      const preferredPolicyIcon =
          preferredList.querySelector('cr-policy-indicator');
      assertTrue(!!preferredPolicyIcon);
      assertEquals(
          internetKnownNetworksPage.i18n(
              'networkA11yManagedByAdministrator', 'wifi2'),
          preferredPolicyIcon.iconAriaLabel);

      const preferredMenuButton =
          preferredList.querySelector<CrIconButtonElement>('.icon-more-vert');
      assertTrue(!!preferredMenuButton);
      assertEquals(
          internetKnownNetworksPage.i18n(
              'knownNetworksMenuButtonTitle', 'wifi2'),
          preferredMenuButton.title);

      const notPreferredList =
          internetKnownNetworksPage.shadowRoot!.querySelector(
              '#notPreferredNetworkList');
      assertTrue(!!notPreferredList);

      const notPreferredPolicyIcon =
          notPreferredList.querySelector('cr-policy-indicator');
      assertTrue(!!notPreferredPolicyIcon);
      assertEquals(
          internetKnownNetworksPage.i18n(
              'networkA11yManagedByAdministrator', 'wifi1'),
          notPreferredPolicyIcon.iconAriaLabel);

      const notPreferredMenuButton =
          notPreferredList.querySelector<CrIconButtonElement>(
              '.icon-more-vert');
      assertTrue(!!notPreferredMenuButton);
      assertEquals(
          internetKnownNetworksPage.i18n(
              'knownNetworksMenuButtonTitle', 'wifi1'),
          notPreferredMenuButton.title);
    });

    test('Passpoint is enabled without subscriptions', async () => {
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      setNetworksForTest([
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append(
          'settingId', settingMojom.Setting.kForgetWifiNetwork.toString());
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
      await flushTasks();

      assertNull(internetKnownNetworksPage.shadowRoot!.querySelector(
          '#passpointSubscriptionList'));
    });

    test('Passpoint is enabled with subscriptions', async () => {
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const firstSubId = 'passpoint_id_1';
      passpointServiceApi.addSubscription({
        id: firstSubId,
        friendlyName: 'My Passpoint provider',
        domains: [],
        provisioningSource: '',
        expirationEpochMs: 0n,
        trustedCa: null,
      });
      passpointServiceApi.addSubscription({
        id: 'passpoint_id_2',
        friendlyName: 'My second Passpoint provider',
        domains: [],
        provisioningSource: '',
        expirationEpochMs: 0n,
        trustedCa: null,
      });
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      setNetworksForTest([
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append(
          'settingId', settingMojom.Setting.kForgetWifiNetwork.toString());
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
      await flushTasks();

      // Check the list is visible and show two subscriptions.
      const list = internetKnownNetworksPage.shadowRoot!.querySelector(
          '#passpointSubscriptionList');
      assertTrue(!!list);
      const items = list.querySelectorAll('div.list-item');
      assertEquals(2, items.length);

      // Check a click on the row sends to the details page.
      const row = items[0]!.querySelector('cr-link-row');
      assertTrue(!!row);
      const showDetailPromise = eventToPromise('show-passpoint-detail', window);
      row.click();
      const showDetailEvent = await showDetailPromise;
      assertEquals(firstSubId, showDetailEvent.detail.id);
    });

    test('Passpoint menu allows removal', async () => {
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      passpointServiceApi.addSubscription({
        id: 'passpoint_id',
        friendlyName: 'My Passpoint provider',
        domains: [],
        provisioningSource: '',
        expirationEpochMs: 0n,
        trustedCa: null,
      });
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      setNetworksForTest([
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append(
          'settingId', settingMojom.Setting.kForgetWifiNetwork.toString());
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
      await flushTasks();

      // Check the list is visible and show two subscriptions.
      const list = internetKnownNetworksPage.shadowRoot!.querySelector(
          '#passpointSubscriptionList');
      assertTrue(!!list);
      const items = list.querySelectorAll('div.list-item');
      assertEquals(1, items.length);

      // Trigger the dots menu.
      const menuButton =
          items[0]!.querySelector<CrIconButtonElement>('.icon-more-vert');
      assertTrue(!!menuButton);
      menuButton.click();
      await waitAfterNextRender(menuButton);

      const menu =
          internetKnownNetworksPage.shadowRoot!
              .querySelector<CrActionMenuElement>('#subscriptionDotsMenu');
      assertTrue(!!menu);
      assertTrue(menu.open);

      const forgetButton =
          menu.querySelector<HTMLButtonElement>('.dropdown-item');
      assertTrue(!!forgetButton);
      forgetButton.click();
      await waitAfterNextRender(forgetButton);

      const resp = await passpointServiceApi.listPasspointSubscriptions();
      assertTrue(resp.result.length === 0);
    });
  });
});
