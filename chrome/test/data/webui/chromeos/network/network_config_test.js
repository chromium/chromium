// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_config.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfigRemote, SecurityType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';

suite('network-config', function() {
  let networkConfig;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  const kCaHash = 'CAHASH';
  const kUserHash1 = 'USERHASH1';
  const kCaPem = 'test-pem';
  const kUserCertId = 'test-cert-id';

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  function setNetworkConfig(properties, prefilledProperties = undefined) {
    assertTrue(!!properties.guid);
    mojoApi_.setManagedPropertiesForTest(properties);
    PolymerTest.clearBody();
    networkConfig = document.createElement('network-config');
    networkConfig.guid = properties.guid;
    networkConfig.managedProperties = properties;

    if (prefilledProperties !== undefined) {
      networkConfig.prefilledProperties = prefilledProperties;
    }
  }

  function setNetworkType(type, security) {
    PolymerTest.clearBody();
    networkConfig = document.createElement('network-config');
    networkConfig.type = OncMojo.getNetworkTypeString(type);
    if (security !== undefined) {
      networkConfig.securityType_ = security;
    }
  }

  function initNetworkConfig() {
    document.body.appendChild(networkConfig);
    networkConfig.init();
    flush();
  }

  function flushAsync() {
    flush();

    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * Simulate an element of id |elementId| fires enter event.
   * @param {string} elementId
   */
  function simulateEnterPressedInElement(elementId) {
    const element = networkConfig.$$(`#${elementId}`);
    networkConfig.connectOnEnter = true;
    assertTrue(!!element);
    element.fire('enter', {path: [element]});
  }

  suite('Share', function() {
    setup(function() {
      mojoApi_.resetForTest();
    });

    teardown(function() {
      PolymerTest.clearBody();
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
      mojoApi_.setGlobalPolicy(globalPolicy);
      await flushAsync();
    }

    test('New Config: Login or guest', function() {
      // Insecure networks are always shared so test a secure config.
      setNetworkType(NetworkType.kWiFi, SecurityType.kWepPsk);
      setLoginOrGuest();
      initNetworkConfig();
      return flushAsync().then(() => {
        const share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test(
        'New Config: Login or guest ephemeral user network configs',
        async function() {
          loadTimeData.overrideValues({
            'ephemeralNetworkPoliciesEnabled': true,
          });

          // Insecure networks are always shared so test a secure config.
          setUserCreatedNetworkConfigurationsAreEphemeral();
          setNetworkType(NetworkType.kWiFi, SecurityType.kWepPsk);
          setLoginOrGuest();
          initNetworkConfig();

          await flushAsync();

          assertFalse(!!networkConfig.$$('#share'));

          const shareEphemeralDisabled =
              networkConfig.$$('#shareEphemeralDisabled');
          assertTrue(!!shareEphemeralDisabled);
          assertFalse(shareEphemeralDisabled.checked);
          assertTrue(!!shareEphemeralDisabled.shadowRoot.querySelector(
              'cr-policy-network-indicator-mojo'));

          // Still, as we're on the login screen, the network config should be
          // persisted in the shared profile.
          assertTrue(networkConfig.shareNetwork_);
        });

    test(
        'New Config: Login or guest disabled ephemeral user network configs',
        async function() {
          loadTimeData.overrideValues({
            'ephemeralNetworkPoliciesEnabled': false,
          });

          // Insecure networks are always shared so test a secure config.
          setUserCreatedNetworkConfigurationsAreEphemeral();
          setNetworkType(NetworkType.kWiFi, SecurityType.kWepPsk);
          setLoginOrGuest();
          initNetworkConfig();

          await flushAsync();

          assertTrue(!!networkConfig.$$('#share'));
          assertFalse(!!networkConfig.$$('#shareEphemeralDisabled'));
        });

    test('New Config: Kiosk', function() {
      // Insecure networks are always shared so test a secure config.
      setNetworkType(NetworkType.kWiFi, SecurityType.kWepPsk);
      setKiosk();
      initNetworkConfig();
      return flushAsync().then(() => {
        const share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Not secure', function() {
      setNetworkType(NetworkType.kWiFi);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        const share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertFalse(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Authenticated, Secure', function() {
      setNetworkType(NetworkType.kWiFi, SecurityType.kWepPsk);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        const share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertFalse(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Secure, ephemeral', async function() {
      loadTimeData.overrideValues({
        'ephemeralNetworkPoliciesEnabled': true,
      });
      setUserCreatedNetworkConfigurationsAreEphemeral();
      setNetworkType(NetworkType.kWiFi, SecurityType.kWepPsk);
      setAuthenticated();
      initNetworkConfig();

      await flushAsync();

      assertFalse(!!networkConfig.$$('#share'));

      const shareEphemeralDisabled =
          networkConfig.$$('#shareEphemeralDisabled');
      assertTrue(!!shareEphemeralDisabled);
      assertFalse(shareEphemeralDisabled.checked);
      assertTrue(!!shareEphemeralDisabled.shadowRoot.querySelector(
          'cr-policy-network-indicator-mojo'));

      // When creating an secure wifi config in a user session, it should be
      // persisted in the user's profile.
      assertFalse(networkConfig.shareNetwork_);
    });

    test('New Config: Authenticated, Not secure, ephemeral', async function() {
      loadTimeData.overrideValues({
        'ephemeralNetworkPoliciesEnabled': true,
      });
      setUserCreatedNetworkConfigurationsAreEphemeral();
      setNetworkType(NetworkType.kWiFi);
      setAuthenticated();
      initNetworkConfig();

      await flushAsync();

      assertFalse(!!networkConfig.$$('#share'));

      const shareEphemeralDisabled =
          networkConfig.$$('#shareEphemeralDisabled');
      assertTrue(!!shareEphemeralDisabled);
      assertFalse(shareEphemeralDisabled.checked);
      assertTrue(!!shareEphemeralDisabled.shadowRoot.querySelector(
          'cr-policy-network-indicator-mojo'));

      // When creating an insecure wifi config in a user session, it is
      // persisted in the shared profile by default.
      assertTrue(networkConfig.shareNetwork_);
    });

    test(
        'New Config: Authenticated, Not secure to secure to not secure',
        async function() {
          // set default to insecure network
          setNetworkType(NetworkType.kWiFi);
          setAuthenticated();
          initNetworkConfig();
          await flushAsync();
          const share = networkConfig.$$('#share');
          assertTrue(!!share);
          assertFalse(share.disabled);
          assertTrue(share.checked);

          // change to secure network
          networkConfig.securityType_ = SecurityType.kWepPsk;
          await flushAsync();
          assertTrue(!!share);
          assertFalse(share.disabled);
          assertFalse(share.checked);

          // change back to insecure network
          networkConfig.securityType_ = SecurityType.kNone;
          await flushAsync();
          assertTrue(!!share);
          assertFalse(share.disabled);
          assertTrue(share.checked);
        });

    // Existing networks hide the shared control in the config UI.
    test('Existing Hides Shared', function() {
      const wifi1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kWiFi, 'someguid', '');
      wifi1.source = OncSource.kUser;
      wifi1.typeProperties.wifi.security = SecurityType.kWepPsk;
      setNetworkConfig(wifi1);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        assertFalse(!!networkConfig.$$('#share'));
      });
    });

    test('Ethernet', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          NetworkType.kEthernet, 'ethernetguid', '');
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('None');
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('ethernetguid', networkConfig.guid);
        assertEquals(SecurityType.kNone, networkConfig.securityType_);
        const outer = networkConfig.$$('#outer');
        assertFalse(!!outer);
      });
    });

    test('Ethernet EAP', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          NetworkType.kEthernet, 'eapguid', '');
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('8021x');
      eth.typeProperties.ethernet.eap = {
        outer: OncMojo.createManagedString('PEAP'),
      };
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('eapguid', networkConfig.guid);
        assertEquals(SecurityType.kWpaEap, networkConfig.securityType_);
        assertEquals(
            'PEAP',
            networkConfig.managedProperties.typeProperties.ethernet.eap.outer
                .activeValue);
        assertEquals(
            'PEAP',
            networkConfig.configProperties_.typeConfig.ethernet.eap.outer);
        assertEquals('PEAP', networkConfig.eapProperties_.outer);
        const outer = networkConfig.$$('#outer');
        assertTrue(!!outer);
        assertTrue(!outer.disabled);
        assertEquals('PEAP', outer.value);
      });
    });

    test('Ethernet input fires enter event on keydown', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          NetworkType.kEthernet, 'eapguid', '');
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('8021x');
      eth.typeProperties.ethernet.eap = {
        outer: OncMojo.createManagedString('PEAP'),
      };
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertFalse(networkConfig.propertiesSent_);
        simulateEnterPressedInElement('oncEAPIdentity');
        assertTrue(networkConfig.propertiesSent_);
      });
    });
  });

  suite('Pre-filled', function() {
    setup(function() {
      mojoApi_.resetForTest();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    function getPrefilledProperties(
        ssid, security, password = undefined, eapConfig = undefined) {
      const properties = OncMojo.getDefaultConfigProperties(NetworkType.kWiFi);
      properties.typeConfig.wifi.ssid = ssid;
      properties.typeConfig.wifi.security = security;
      properties.typeConfig.wifi.passphrase = password;
      properties.typeConfig.wifi.eap = eapConfig;
      return properties;
    }

    test('None', function() {
      setNetworkType(NetworkType.kWiFi, SecurityType.kWepPsk);
      initNetworkConfig();

      return flushAsync().then(() => {
        const ssid = networkConfig.$$('#ssid');
        assertTrue(!!ssid);
        assertFalse(ssid.readonly);

        const security = networkConfig.$$('#security');
        assertTrue(!!security);
        assertFalse(security.disabled);

        const password = networkConfig.$$('#wifi-passphrase');
        assertTrue(!!password);
        assertFalse(password.readonly);
      });
    });

    test('Insecure', function() {
      const testSsid = 'somessid';
      const wifi = OncMojo.getDefaultManagedProperties(
          NetworkType.kWiFi, 'someguid', '');
      const prefilledProperties =
          getPrefilledProperties(testSsid, SecurityType.kNone);
      setNetworkConfig(wifi, prefilledProperties);
      initNetworkConfig();

      return flushAsync().then(() => {
        const ssid = networkConfig.$$('#ssid');
        assertTrue(!!ssid);
        assertTrue(ssid.readonly);
        assertEquals(testSsid, ssid.value);

        const security = networkConfig.$$('#security');
        assertTrue(!!security);
        assertTrue(security.disabled);
        assertEquals(SecurityType.kNone, security.value);
      });
    });

    test('Secure', function() {
      const testSsid = 'somessid';
      const testPassword = 'somepassword';
      const wifi = OncMojo.getDefaultManagedProperties(
          NetworkType.kWiFi, 'someguid', '');
      const prefilledProperties =
          getPrefilledProperties(testSsid, SecurityType.kWpaPsk, testPassword);
      setNetworkConfig(wifi, prefilledProperties);
      initNetworkConfig();

      return flushAsync().then(() => {
        const ssid = networkConfig.$$('#ssid');
        assertTrue(!!ssid);
        assertTrue(ssid.readonly);
        assertEquals(testSsid, ssid.value);

        const security = networkConfig.$$('#security');
        assertTrue(!!security);
        assertTrue(security.disabled);
        assertEquals(SecurityType.kWpaPsk, security.value);

        const password = networkConfig.$$('#wifi-passphrase');
        assertTrue(!!password);
        assertTrue(password.readonly);
        assertEquals(testPassword, password.value);
      });
    });

    test('Secure EAP', function() {
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
          testSsid, SecurityType.kWpaEap, testPassword, testEapConfig);
      setNetworkConfig(wifi, prefilledProperties);
      initNetworkConfig();

      return flushAsync().then(() => {
        const ssid = networkConfig.$$('#ssid');
        assertTrue(!!ssid);
        assertTrue(ssid.readonly);
        assertEquals(testSsid, ssid.value);

        const security = networkConfig.$$('#security');
        assertTrue(!!security);
        assertTrue(security.disabled);
        assertEquals(SecurityType.kWpaEap, security.value);

        const eapPassword = networkConfig.$$('#eapPassword');
        assertTrue(!!eapPassword);
        assertTrue(eapPassword.readonly);
        assertEquals(testPassword, eapPassword.value);

        const eapOuter = networkConfig.$$('#outer');
        assertTrue(!!eapOuter);
        assertTrue(eapOuter.disabled);
        assertEquals(testOuter, eapOuter.value);

        const eapInner = networkConfig.$$('#inner');
        assertTrue(!!eapInner);
        assertTrue(eapInner.disabled);
        assertEquals(testInner, eapInner.value);

        const eapIdentity = networkConfig.$$('#oncEAPIdentity');
        assertTrue(!!eapIdentity);
        assertTrue(eapIdentity.readonly);
        assertEquals(testIdentity, eapIdentity.value);

        const eapAnonymousIdentity =
            networkConfig.$$('#oncEAPAnonymousIdentity');
        assertTrue(!!eapAnonymousIdentity);
        assertTrue(eapAnonymousIdentity.readonly);
        assertEquals(testAnonymousIdentity, eapAnonymousIdentity.value);
      });
    });
  });
});
