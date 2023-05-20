// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {MojoConnectivityProvider} from 'chrome://resources/ash/common/connectivity/mojo_connectivity_provider.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakePasspointService} from 'chrome://webui-test/chromeos/fake_passpoint_service_mojom.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';


suite('InternetKnownNetworksPage', function() {
  /** @type {?SettingsInternetKnownNetworksPageElement} */
  let internetKnownNetworksPage = null;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  /** @type {?PasspointServiceRemote} */
  let passpointServiceApi_ = null;

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
    passpointServiceApi_ = new FakePasspointService();
    MojoConnectivityProvider.getInstance().setPasspointServiceForTest(
        passpointServiceApi_);

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
    mojoApi_.addNetworksForTest(networks);
  }

  setup(function() {
    PolymerTest.clearBody();
    mojoApi_.resetForTest();
    passpointServiceApi_.resetForTest();
    return flushAsync();
  });

  teardown(function() {
    internetKnownNetworksPage.remove();
    internetKnownNetworksPage = null;
    Router.getInstance().resetRouteForTesting();
  });

  function init() {
    internetKnownNetworksPage =
        document.createElement('settings-internet-known-networks-subpage');
    assertTrue(!!internetKnownNetworksPage);
    document.body.appendChild(internetKnownNetworksPage);
    return flushAsync();
  }

  suite('KnownNetworksPage', function() {
    test('WiFi', async () => {
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      setNetworksForTest(NetworkType.kWiFi, [
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append('settingId', '7');
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);

      await flushAsync();

      assertEquals(2, internetKnownNetworksPage.networkStateList_.length);

      const preferredList = internetKnownNetworksPage.shadowRoot.querySelector(
          '#preferredNetworkList');
      assertTrue(!!preferredList);
      const preferredElems = preferredList.querySelectorAll('cr-link-row');
      assertEquals(preferredElems.length, 1);

      const deepLinkElement =
          preferredElems[0].shadowRoot.querySelector('#icon');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender();
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Preferred list elem should be focused for settingId=7.');
    });

    test('Known networks policy icon and menu button a11y', async () => {
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      preferredWifi.source = OncSource.kDevicePolicy;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      notPreferredWifi.source = OncSource.kDevicePolicy;
      setNetworksForTest(NetworkType.kWiFi, [
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append('settingId', '7');
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);

      await flushAsync();

      assertEquals(2, internetKnownNetworksPage.networkStateList_.length);
      const preferredList = internetKnownNetworksPage.shadowRoot.querySelector(
          '#preferredNetworkList');
      assertTrue(!!preferredList);

      const preferredPolicyIcon =
          preferredList.querySelector('cr-policy-indicator');
      assertTrue(!!preferredPolicyIcon);
      assertEquals(
          preferredPolicyIcon.iconAriaLabel,
          internetKnownNetworksPage.i18n(
              'networkA11yManagedByAdministrator', 'wifi2'));

      const preferredMenuButton =
          preferredList.querySelector('.icon-more-vert');
      assertTrue(!!preferredMenuButton);
      assertEquals(
          preferredMenuButton.title,
          internetKnownNetworksPage.i18n(
              'knownNetworksMenuButtonTitle', 'wifi2'));

      const notPreferredList =
          internetKnownNetworksPage.shadowRoot.querySelector(
              '#notPreferredNetworkList');
      assertTrue(!!notPreferredList);

      const notPreferredPolicyIcon =
          notPreferredList.querySelector('cr-policy-indicator');
      assertTrue(!!notPreferredPolicyIcon);
      assertEquals(
          notPreferredPolicyIcon.iconAriaLabel,
          internetKnownNetworksPage.i18n(
              'networkA11yManagedByAdministrator', 'wifi1'));

      const notPreferredMenuButton =
          notPreferredList.querySelector('.icon-more-vert');
      assertTrue(!!notPreferredMenuButton);
      assertEquals(
          notPreferredMenuButton.title,
          internetKnownNetworksPage.i18n(
              'knownNetworksMenuButtonTitle', 'wifi1'));
    });

    test('Passpoint is disabled', async () => {
      loadTimeData.overrideValues({isPasspointSettingsEnabled: false});
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      passpointServiceApi_.addSubscription({
        id: 'a_passpoint_id',
        friendlyName: 'My Passpoint provider',
      });
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      setNetworksForTest(NetworkType.kWiFi, [
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append('settingId', '7');
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);

      await flushAsync();

      assertFalse(!!internetKnownNetworksPage.shadowRoot.querySelector(
          '#passpointSubscriptionList'));
    });

    test('Passpoint is enabled without subscriptions', async () => {
      loadTimeData.overrideValues({isPasspointSettingsEnabled: true});
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      setNetworksForTest(NetworkType.kWiFi, [
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append('settingId', '7');
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);

      await flushAsync();

      assertFalse(!!internetKnownNetworksPage.shadowRoot.querySelector(
          '#passpointSubscriptionList'));
    });

    test('Passpoint is enabled with subscriptions', async () => {
      loadTimeData.overrideValues({isPasspointSettingsEnabled: true});
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      const firstSubId = 'passpoint_id_1';
      passpointServiceApi_.addSubscription({
        id: firstSubId,
        friendlyName: 'My Passpoint provider',
      });
      passpointServiceApi_.addSubscription({
        id: 'passpoint_id_2',
        friendlyName: 'My second Passpoint provider',
      });
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      setNetworksForTest(NetworkType.kWiFi, [
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append('settingId', '7');
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);

      await flushAsync();

      // Check the list is visible and show two subscriptions.
      const list = internetKnownNetworksPage.shadowRoot.querySelector(
          '#passpointSubscriptionList');
      assertTrue(!!list);
      const items = list.querySelectorAll('div.list-item');
      assertEquals(2, items.length);

      // Check a click on the row sends to the details page.
      const row = items[0].querySelector('cr-link-row');
      assertTrue(!!row);
      const showDetailPromise = eventToPromise('show-passpoint-detail', window);
      row.click();
      const showDetailEvent = await showDetailPromise;
      assertEquals(firstSubId, showDetailEvent.detail.id);
    });

    test('Passpoint menu allows removal', async () => {
      loadTimeData.overrideValues({isPasspointSettingsEnabled: true});
      await init();
      internetKnownNetworksPage.networkType = NetworkType.kWiFi;
      mojoApi_.setNetworkTypeEnabledState(NetworkType.kWiFi, true);
      passpointServiceApi_.addSubscription({
        id: 'passpoint_id',
        friendlyName: 'My Passpoint provider',
      });
      const preferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi2');
      preferredWifi.priority = 1;
      const notPreferredWifi =
          OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
      setNetworksForTest(NetworkType.kWiFi, [
        notPreferredWifi,
        preferredWifi,
      ]);

      const params = new URLSearchParams();
      params.append('settingId', '7');
      Router.getInstance().navigateTo(routes.KNOWN_NETWORKS, params);
      await flushAsync();

      // Check the list is visible and show two subscriptions.
      const list = internetKnownNetworksPage.shadowRoot.querySelector(
          '#passpointSubscriptionList');
      assertTrue(!!list);
      const items = list.querySelectorAll('div.list-item');
      assertEquals(1, items.length);

      // Trigger the dots menu.
      const menuButton = items[0].querySelector('.icon-more-vert');
      assertTrue(!!menuButton);
      menuButton.click();
      await waitAfterNextRender(menuButton);

      const menu = internetKnownNetworksPage.shadowRoot.querySelector(
          '#subscriptionDotsMenu');
      assertTrue(!!menu);
      assertTrue(menu.open);
    });
  });
});
