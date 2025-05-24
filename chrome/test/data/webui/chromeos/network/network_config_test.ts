// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_config.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import type {NetworkConfigElement} from 'chrome://resources/ash/common/network/network_config.js';
import type {NetworkConfigInputElement} from 'chrome://resources/ash/common/network/network_config_input.js';
import type {NetworkConfigSelectElement} from 'chrome://resources/ash/common/network/network_config_select.js';
import type {NetworkConfigToggleElement} from 'chrome://resources/ash/common/network/network_config_toggle.js';
import type {NetworkPasswordInputElement} from 'chrome://resources/ash/common/network/network_password_input.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {ConfigProperties, EAPConfigProperties, GlobalPolicy, ManagedEAPProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {SecurityType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

import {clearBody, createNetworkConfigWithNetworkType, createNetworkConfigWithProperties, simulateEnterPressedInElement} from './test_utils.js';

suite('network-config', () => {
  let networkConfig: NetworkConfigElement;

  let mojoApi_: FakeNetworkConfig;

  suiteSetup(() => {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi_);
  });


  function initNetworkConfig(): void {
    document.body.appendChild(networkConfig);
    networkConfig.init();
    flush();
  }

  suite('Share', () => {
    setup(() => {
      mojoApi_.resetForTest();
    });

    teardown(() => {
      clearBody();
    });

    function setLoginOrGuest() {
      // Networks must be shared.
      networkConfig.shareAllowEnable = false;
      networkConfig.shareDefault = true;
    }

    function setKiosk() {
      // New networks can not be shared.
      networkConfig.shareAllowEnable = false;
      networkConfig.shareDefault = false;
    }

    function setAuthenticated() {
      // Logged in users can share new networks.
      networkConfig.shareAllowEnable = true;
      // Authenticated networks default to not shared.
      networkConfig.shareDefault = false;
    }

    async function setUserCreatedNetworkConfigurationsAreEphemeral() {
      const globalPolicy = {
        userCreatedNetworkConfigurationsAreEphemeral: true,
      };
      mojoApi_.setGlobalPolicy(globalPolicy as GlobalPolicy);
      await flushTasks();
    }

    test('New Config: Login or guest', () => {
      // Insecure networks are always shared so test a secure config.
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWepPsk);
      setLoginOrGuest();
      initNetworkConfig();
      return flushTasks().then(() => {
        const share =
            networkConfig.shadowRoot!.querySelector<NetworkConfigToggleElement>(
                '#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test(
        'New Config: Login or guest ephemeral user network configs',
        async () => {
          loadTimeData.overrideValues({
            'ephemeralNetworkPoliciesEnabled': true,
          });

          // Insecure networks are always shared so test a secure config.
          setUserCreatedNetworkConfigurationsAreEphemeral();
          networkConfig = createNetworkConfigWithNetworkType(
              NetworkType.kWiFi, SecurityType.kWepPsk);
          setLoginOrGuest();
          initNetworkConfig();

          await flushTasks();

          assertFalse(
              !!networkConfig.shadowRoot!
                    .querySelector<NetworkConfigToggleElement>('#share'));

          const shareEphemeralDisabled =
              networkConfig.shadowRoot!
                  .querySelector<NetworkConfigToggleElement>(
                      '#shareEphemeralDisabled');
          assertTrue(!!shareEphemeralDisabled);
          assertFalse(shareEphemeralDisabled.checked);
          assertTrue(!!shareEphemeralDisabled.shadowRoot!.querySelector(
              'cr-policy-network-indicator-mojo'));

          // Still, as we're on the login screen, the network config should be
          // persisted in the shared profile.
          assertTrue(networkConfig.getShareNetworkForTesting());
        });

    test(
        'New Config: Login or guest disabled ephemeral user network configs',
        async () => {
          loadTimeData.overrideValues({
            'ephemeralNetworkPoliciesEnabled': false,
          });

          // Insecure networks are always shared so test a secure config.
          setUserCreatedNetworkConfigurationsAreEphemeral();
          networkConfig = createNetworkConfigWithNetworkType(
              NetworkType.kWiFi, SecurityType.kWepPsk);
          setLoginOrGuest();
          initNetworkConfig();

          await flushTasks();

          assertTrue(
              !!networkConfig.shadowRoot!
                    .querySelector<NetworkConfigToggleElement>('#share'));
          assertFalse(!!networkConfig.shadowRoot!
                            .querySelector<NetworkConfigToggleElement>(
                                '#shareEphemeralDisabled'));
        });

    test('New Config: Kiosk', () => {
      // Insecure networks are always shared so test a secure config.
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWepPsk);
      setKiosk();
      initNetworkConfig();
      return flushTasks().then(() => {
        const share =
            networkConfig.shadowRoot!.querySelector<NetworkConfigToggleElement>(
                '#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Not secure', () => {
      networkConfig = createNetworkConfigWithNetworkType(NetworkType.kWiFi);
      setAuthenticated();
      initNetworkConfig();
      return flushTasks().then(() => {
        const share =
            networkConfig.shadowRoot!.querySelector<NetworkConfigToggleElement>(
                '#share');
        assertTrue(!!share);
        assertFalse(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Authenticated, Secure', () => {
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWepPsk);
      setAuthenticated();
      initNetworkConfig();
      return flushTasks().then(() => {
        const share =
            networkConfig.shadowRoot!.querySelector<NetworkConfigToggleElement>(
                '#share');
        assertTrue(!!share);
        assertFalse(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Secure, ephemeral', async () => {
      loadTimeData.overrideValues({
        'ephemeralNetworkPoliciesEnabled': true,
      });
      setUserCreatedNetworkConfigurationsAreEphemeral();
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWepPsk);
      setAuthenticated();
      initNetworkConfig();

      await flushTasks();

      assertFalse(
          !!networkConfig.shadowRoot!.querySelector<NetworkConfigToggleElement>(
              '#share'));

      const shareEphemeralDisabled =
          networkConfig.shadowRoot!.querySelector<NetworkConfigToggleElement>(
              '#shareEphemeralDisabled');
      assertTrue(!!shareEphemeralDisabled);
      assertFalse(shareEphemeralDisabled.checked);
      assertTrue(!!shareEphemeralDisabled.shadowRoot!.querySelector(
          'cr-policy-network-indicator-mojo'));

      // When creating an secure wifi config in a user session, it should be
      // persisted in the user's profile.
      assertFalse(networkConfig.getShareNetworkForTesting());
    });

    test('New Config: Authenticated, Not secure, ephemeral', async () => {
      loadTimeData.overrideValues({
        'ephemeralNetworkPoliciesEnabled': true,
      });
      setUserCreatedNetworkConfigurationsAreEphemeral();
      networkConfig = createNetworkConfigWithNetworkType(NetworkType.kWiFi);
      setAuthenticated();
      initNetworkConfig();

      await flushTasks();

      assertFalse(
          !!networkConfig.shadowRoot!.querySelector<NetworkConfigToggleElement>(
              '#share'));

      const shareEphemeralDisabled =
          networkConfig.shadowRoot!.querySelector<NetworkConfigToggleElement>(
              '#shareEphemeralDisabled');
      assertTrue(!!shareEphemeralDisabled);
      assertFalse(shareEphemeralDisabled.checked);
      assertTrue(!!shareEphemeralDisabled.shadowRoot!.querySelector(
          'cr-policy-network-indicator-mojo'));

      // When creating an insecure wifi config in a user session, it is
      // persisted in the shared profile by default.
      assertTrue(networkConfig.getShareNetworkForTesting());
    });

    test(
        'New Config: Authenticated, Not secure to secure to not secure',
        async () => {
          // set default to insecure network
          networkConfig = createNetworkConfigWithNetworkType(NetworkType.kWiFi);
          setAuthenticated();
          initNetworkConfig();
          await flushTasks();
          const share =
              networkConfig.shadowRoot!
                  .querySelector<NetworkConfigToggleElement>('#share');
          assertTrue(!!share);
          assertFalse(share.disabled);
          assertTrue(share.checked);

          // change to secure network
          networkConfig.setSecurityTypeForTesting(SecurityType.kWepPsk);
          await flushTasks();
          assertTrue(!!share);
          assertFalse(share.disabled);
          assertFalse(share.checked);

          // change back to insecure network
          networkConfig.setSecurityTypeForTesting(SecurityType.kNone);
          await flushTasks();
          assertTrue(!!share);
          assertFalse(share.disabled);
          assertTrue(share.checked);
        });

    // Existing networks hide the shared control in the config UI.
    test('Existing Hides Shared', () => {
      const wifi1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kWiFi, 'someguid', '');
      wifi1.source = OncSource.kUser;
      assertTrue(!!wifi1.typeProperties.wifi);
      wifi1.typeProperties.wifi.security = SecurityType.kWepPsk;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, wifi1);
      setAuthenticated();
      initNetworkConfig();
      return flushTasks().then(() => {
        assertFalse(!!networkConfig.shadowRoot!
                          .querySelector<NetworkConfigToggleElement>('#share'));
      });
    });

    test('Ethernet', () => {
      const eth = OncMojo.getDefaultManagedProperties(
          NetworkType.kEthernet, 'ethernetguid', '');
      assertTrue(!!eth.typeProperties.ethernet);
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('None');
      networkConfig = createNetworkConfigWithProperties(mojoApi_, eth);
      initNetworkConfig();
      return flushTasks().then(() => {
        assertEquals('ethernetguid', networkConfig.guid);
        assertEquals(
            SecurityType.kNone, networkConfig.getSecurityTypeForTesting());
        const outer =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#outer');
        assertFalse(!!outer);
      });
    });

    test('Ethernet EAP', () => {
      const eth = OncMojo.getDefaultManagedProperties(
          NetworkType.kEthernet, 'eapguid', '');
      assertTrue(!!eth.typeProperties.ethernet);
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('8021x');
      const eapProperties = {
        outer: OncMojo.createManagedString('PEAP'),
      };
      eth.typeProperties.ethernet.eap = eapProperties as ManagedEAPProperties;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, eth);
      initNetworkConfig();
      return flushTasks().then(() => {
        assertEquals('eapguid', networkConfig.guid);
        assertEquals(
            SecurityType.kWpaEap, networkConfig.getSecurityTypeForTesting());
        const outer =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#outer');
        assertTrue(!!outer);
        assertTrue(!outer.disabled);
        assertEquals('PEAP', outer.value);
      });
    });

    test('Ethernet input fires enter event on keydown', () => {
      const eth = OncMojo.getDefaultManagedProperties(
          NetworkType.kEthernet, 'eapguid', '');
      assertTrue(!!eth.typeProperties.ethernet);
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('8021x');
      assertTrue(!!eth.typeProperties.ethernet);
      const eapProperties = {
        outer: OncMojo.createManagedString('PEAP'),
      };
      eth.typeProperties.ethernet.eap = eapProperties as ManagedEAPProperties;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, eth);
      initNetworkConfig();
      return flushTasks().then(() => {
        assertFalse(networkConfig.getPropertiesSentForTesting());
        simulateEnterPressedInElement(networkConfig, 'oncEAPIdentity');
        assertTrue(networkConfig.getPropertiesSentForTesting());
      });
    });
  });

  suite('Pre-filled', () => {
    setup(() => {
      mojoApi_.resetForTest();
    });

    teardown(() => {
      clearBody();
    });

    function getPrefilledProperties(
        ssid: string, security: SecurityType,
        password: string|undefined = undefined,
        eapConfig: EAPConfigProperties|undefined =
            undefined): ConfigProperties {
      const properties = OncMojo.getDefaultConfigProperties(NetworkType.kWiFi);
      assertTrue(!!properties.typeConfig.wifi);
      properties.typeConfig.wifi.ssid = ssid;
      properties.typeConfig.wifi.security = security;
      properties.typeConfig.wifi.passphrase = password || null;
      properties.typeConfig.wifi.eap = eapConfig || null;
      return properties;
    }

    test('None', () => {
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWepPsk);
      initNetworkConfig();

      return flushTasks().then(() => {
        const ssid =
            networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
                '#ssid');
        assertTrue(!!ssid);
        assertFalse(ssid.readonly);

        const security =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#security');
        assertTrue(!!security);
        assertFalse(security.disabled);

        const password =
            networkConfig.shadowRoot!
                .querySelector<NetworkPasswordInputElement>('#wifi-passphrase');
        assertTrue(!!password);
        assertFalse(password.readonly);
      });
    });

    test('Insecure', () => {
      const testSsid = 'somessid';
      const wifi = OncMojo.getDefaultManagedProperties(
          NetworkType.kWiFi, 'someguid', '');
      const prefilledProperties =
          getPrefilledProperties(testSsid, SecurityType.kNone);
      networkConfig = createNetworkConfigWithProperties(
          mojoApi_, wifi, prefilledProperties);
      initNetworkConfig();

      return flushTasks().then(() => {
        const ssid =
            networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
                '#ssid');
        assertTrue(!!ssid);
        assertTrue(ssid.readonly);
        assertEquals(testSsid, ssid.value);

        const security =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#security');
        assertTrue(!!security);
        assertTrue(security.disabled);
        assertEquals(SecurityType.kNone, security.value);
      });
    });

    test('Secure', () => {
      const testSsid = 'somessid';
      const testPassword = 'somepassword';
      const wifi = OncMojo.getDefaultManagedProperties(
          NetworkType.kWiFi, 'someguid', '');
      const prefilledProperties =
          getPrefilledProperties(testSsid, SecurityType.kWpaPsk, testPassword);
      networkConfig = createNetworkConfigWithProperties(
          mojoApi_, wifi, prefilledProperties);
      initNetworkConfig();

      return flushTasks().then(() => {
        const ssid =
            networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
                '#ssid');
        assertTrue(!!ssid);
        assertTrue(ssid.readonly);
        assertEquals(testSsid, ssid.value);

        const security =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#security');
        assertTrue(!!security);
        assertTrue(security.disabled);
        assertEquals(SecurityType.kWpaPsk, security.value);

        const password =
            networkConfig.shadowRoot!
                .querySelector<NetworkPasswordInputElement>('#wifi-passphrase');
        assertTrue(!!password);
        assertTrue(password.readonly);
        assertEquals(testPassword, password.value);
      });
    });

    test('Secure EAP', () => {
      const testSsid = 'somessid';
      const testPassword = 'somepassword';
      const testAnonymousIdentity = 'testid1';
      const testIdentity = 'testid2';
      const testInner = 'MSCHAPv2';
      const testOuter = 'PEAP';
      const testEapConfig = {
        anonymousIdentity: testAnonymousIdentity,
        identity: testIdentity,
        inner: testInner,
        outer: testOuter,
        password: testPassword,
      };
      const wifi = OncMojo.getDefaultManagedProperties(
          NetworkType.kWiFi, 'someguid', '');
      const prefilledProperties = getPrefilledProperties(
          testSsid, SecurityType.kWpaEap, testPassword,
          testEapConfig as EAPConfigProperties);
      networkConfig = createNetworkConfigWithProperties(
          mojoApi_, wifi, prefilledProperties);
      initNetworkConfig();

      return flushTasks().then(() => {
        const ssid =
            networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
                '#ssid');
        assertTrue(!!ssid);
        assertTrue(ssid.readonly);
        assertEquals(testSsid, ssid.value);

        const security =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#security');
        assertTrue(!!security);
        assertTrue(security.disabled);
        assertEquals(SecurityType.kWpaEap, security.value);

        const eapPassword =
            networkConfig.shadowRoot!
                .querySelector<NetworkPasswordInputElement>('#eapPassword');
        assertTrue(!!eapPassword);
        assertTrue(eapPassword.readonly);
        assertEquals(testPassword, eapPassword.value);

        const eapOuter =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#outer');
        assertTrue(!!eapOuter);
        assertTrue(eapOuter.disabled);
        assertEquals(testOuter, eapOuter.value);

        const eapInner =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#inner');
        assertTrue(!!eapInner);
        assertTrue(eapInner.disabled);
        assertEquals(testInner, eapInner.value);

        const eapIdentity =
            networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
                '#oncEAPIdentity');
        assertTrue(!!eapIdentity);
        assertTrue(eapIdentity.readonly);
        assertEquals(testIdentity, eapIdentity.value);

        const eapAnonymousIdentity =
            networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
                '#oncEAPAnonymousIdentity');
        assertTrue(!!eapAnonymousIdentity);
        assertTrue(eapAnonymousIdentity.readonly);
        assertEquals(testAnonymousIdentity, eapAnonymousIdentity.value);
      });
    });
  });
});
