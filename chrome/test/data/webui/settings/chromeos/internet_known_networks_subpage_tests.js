// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('InternetKnownNetworksPage', function() {
  /** @type {?SettingsInternetKnownNetworksPageElement} */
  let internetKnownNetworksPage = null;

  /** @type {?CrosNetworkConfigRemote} */
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

  function setNetworksForTest(type, networks) {
    mojoApi_.resetForTest();
    mojoApi_.addNetworksForTest(networks);
  }

  setup(function() {
    PolymerTest.clearBody();
    internetKnownNetworksPage =
        document.createElement('settings-internet-known-networks-subpage');
    assertTrue(!!internetKnownNetworksPage);
    mojoApi_.resetForTest();
    document.body.appendChild(internetKnownNetworksPage);
    return flushAsync();
  });

  teardown(function() {
    internetKnownNetworksPage.remove();
    internetKnownNetworksPage = null;
    Router.getInstance().resetRouteForTesting();
  });

  suite('KnownNetworksPage', function() {
    test('WiFi', async () => {
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
  });
});
