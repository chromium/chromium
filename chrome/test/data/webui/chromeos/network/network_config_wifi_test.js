// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_config.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfigRemote, HiddenSsidMode, SecurityType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, OncSource, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';

suite('network-config-wifi', function() {
  let networkConfig;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  const kCaHash = 'CAHASH';
  const kCaPem = 'test-pem';
  const kUserHash1 = 'USERHASH1';
  const kUserHash2 = 'USERHASH2';
  const kUserCertId = 'test-cert-id';
  const kMissedEapDataErr = 'missingEapDefaultServerCaSubjectVerification';
  const kServerCaCertsNotProvided = false;
  const kAddServerCaCerts = true;

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
  });

  function setNetworkConfig(properties) {
    assertTrue(!!properties.guid);
    mojoApi_.setManagedPropertiesForTest(properties);
    PolymerTest.clearBody();
    networkConfig = document.createElement('network-config');
    networkConfig.guid = properties.guid;
    networkConfig.managedProperties = properties;
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

  function initNetworkConfigWithCerts(hasServerCa, hasUserCert) {
    const serverCas = [];
    const userCerts = [];
    if (hasServerCa) {
      serverCas.push({
        hash: kCaHash,
        pemOrId: kCaPem,
        availableForNetworkAuth: true,
        hardwareBacked: true,
        deviceWide: true,
      });
    }
    if (hasUserCert) {
      userCerts.push({
        hash: kUserHash1,
        pemOrId: kUserCertId,
        availableForNetworkAuth: true,
        hardwareBacked: true,
        deviceWide: false,
      });
    }
    mojoApi_.setCertificatesForTest(serverCas, userCerts);
    initNetworkConfig();
  }

  function flushAsync() {
    flush();
    return new Promise(resolve => {
      networkConfig.async(resolve);
    });
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

  suite('New WiFi Config', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(NetworkType.kWiFi);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Default', function() {
      assertTrue(!!networkConfig.$$('#share'));
      assertTrue(!!networkConfig.$$('#ssid'));
      assertTrue(!!networkConfig.$$('#security'));
      assertFalse(networkConfig.$$('#security').disabled);
    });

    test('Passphrase field shows', function() {
      assertFalse(!!networkConfig.$$('#wifi-passphrase'));
      networkConfig.$$('#security').value = SecurityType.kWpaPsk;
      return flushAsync().then(() => {
        assertTrue(!!networkConfig.$$('#wifi-passphrase'));
      });
    });

    test('New networks are explicitly not hidden', async () => {
      networkConfig.save();

      await flushAsync();

      const props = mojoApi_.getPropertiesToSetForTest();
      assertEquals(props.typeConfig.wifi.hiddenSsid, HiddenSsidMode.kDisabled);
    });
  });

  suite('Existing WiFi Config', function() {
    setup(function() {
      mojoApi_.resetForTest();
      const wifi1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kWiFi, 'someguid', '');
      wifi1.name = OncMojo.createManagedString('somename');
      wifi1.source = OncSource.kDevice;
      wifi1.typeProperties.wifi.security = SecurityType.kWepPsk;
      wifi1.typeProperties.wifi.ssid.activeValue = '11111111111';
      wifi1.typeProperties.wifi.passphrase = {activeValue: 'test_passphrase'};
      setNetworkConfig(wifi1);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Default', function() {
      return flushAsync().then(() => {
        assertEquals('someguid', networkConfig.managedProperties.guid);
        assertEquals(
            'somename', networkConfig.managedProperties.name.activeValue);
        assertFalse(!!networkConfig.$$('#share'));
        assertTrue(!!networkConfig.$$('#ssid'));
        assertTrue(!!networkConfig.$$('#security'));
        assertTrue(networkConfig.$$('#security').disabled);
      });
    });

    test('WiFi input fires enter event on keydown', function() {
      return flushAsync().then(() => {
        assertFalse(networkConfig.propertiesSent_);
        simulateEnterPressedInElement('ssid');
        assertTrue(networkConfig.propertiesSent_);
      });
    });

    test('Remove error text when input key is pressed', function() {
      return flushAsync().then(() => {
        networkConfig.error = 'bad-passphrase';
        const passwordInput = networkConfig.$$('#wifi-passphrase');
        assertTrue(!!passwordInput);
        assertTrue(!!networkConfig.error);

        passwordInput.fire('keypress');
        flush();
        assertFalse(!!networkConfig.error);
      });
    });

    test('Networks\' hidden SSID mode is not overwritten', async () => {
      await flushAsync();

      networkConfig.save();

      await flushAsync();

      const props = mojoApi_.getPropertiesToSetForTest();
      assertEquals(props.typeConfig.wifi.hiddenSsid, HiddenSsidMode.kAutomatic);
    });
  });

  suite('Non-VPN EAP', function() {
    setup(function() {
      mojoApi_.resetForTest();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    function setAuthenticated() {
      // Logged in users can share new networks.
      networkConfig.shareAllowEnable = true;
      // Authenticated networks default to not shared.
      networkConfig.shareDefault = false;
    }

    async function saveNetworkConfig() {
      networkConfig.save();
      await flushAsync();
    }

    async function setSerializedSubjectAltNameMatch(value) {
      networkConfig.serializedSubjectAltNameMatch_ = value;
      await saveNetworkConfig();
    }

    async function setSerializedDomainSuffixMatch(value) {
      networkConfig.serializedDomainSuffixMatch_ = value;
      await saveNetworkConfig();
    }

    function isDefaultServerCaSelected() {
      return 'default' === networkConfig.selectedServerCaHash_;
    }

    function isConfigErrorsPresent() {
      return '' !== networkConfig.error;
    }

    function isMissedEapDataErrorShown() {
      return kMissedEapDataErr === networkConfig.error;
    }

    function getErrorMessage(eapType) {
      return 'Failed test for eapType = ' + eapType;
    }

    async function initiateWiFiEapConfig(isAddServerCA, eapType) {
      setNetworkType(NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfigWithCerts(
          /* hasServerCa= */ isAddServerCA, /* hasUserCert= */ true);
      networkConfig.shareNetwork_ = true;
      networkConfig.set('eapProperties_.outer', eapType);
      await mojoApi_.whenCalled('getNetworkCertificates');
      networkConfig.save();
      await flushAsync();
    }

    test('WiFi EAP Default Outer', async function() {
      setNetworkType(NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfig();
      networkConfig.shareNetwork_ = false;
      await mojoApi_.whenCalled('getNetworkCertificates');
      await flushAsync();
      const outer = networkConfig.$$('#outer');
      // 'PEAP' should be the default 'Outer' protocol.
      assertEquals('PEAP', outer.value);
    });

    test('WiFi EAP-TLS No Certs', function() {
      setNetworkType(NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfig();
      networkConfig.shareNetwork_ = false;
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          const outer = networkConfig.$$('#outer');
          assertEquals('EAP-TLS', outer.value);
          // Check that with no certificates, 'default' and 'no-certs' are
          // selected.
          assertEquals('default', networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);
        });
      });
    });

    test('WiFi EAP-TLS Certs', function() {
      setNetworkType(NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true,
          }],
          [{
            hash: kUserHash1,
            pemOrId: kUserCertId,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: false,
          }]);
      initNetworkConfig();
      networkConfig.shareNetwork_ = false;
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // The first Server CA  and User certificate should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);
        });
      });
    });

    test('WiFi EAP-TLS Certs Shared', function() {
      setNetworkType(NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true,
          }],
          [
            {
              hash: kUserHash1,
              pemOrId: kUserCertId,
              availableForNetworkAuth: true,
              hardwareBacked: true,
              deviceWide: false,
            },
            {
              hash: kUserHash2,
              pemOrId: kUserCertId,
              availableForNetworkAuth: true,
              hardwareBacked: true,
              deviceWide: true,
            },
          ]);
      initNetworkConfig();
      networkConfig.shareNetwork_ = true;
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // The first Server CA should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          // Second User Hash should be selected since it is a device cert.
          assertEquals(kUserHash2, networkConfig.selectedUserCertHash_);
        });
      });
    });

    test('WiFi PEAP No Certs', async function() {
      setNetworkType(NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfig();
      networkConfig.shareNetwork_ = false;
      networkConfig.set('eapProperties_.outer', 'PEAP');
      await mojoApi_.whenCalled('getNetworkCertificates');
      await flushAsync();
      const outer = networkConfig.$$('#outer');
      assertEquals('PEAP', outer.value);
      // 'default' Server CA should be selected in case of no certificates
      assertEquals('default', networkConfig.selectedServerCaHash_);
      // 'no-user-cert' is selected as user certificate because a
      // user certificate is not needed for PEAP
      assertEquals('no-user-cert', networkConfig.selectedUserCertHash_);
    });

    test('WiFi PEAP Certs', async function() {
      setNetworkType(NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true,
          }],
          [{
            hash: kUserHash1,
            pemOrId: kUserCertId,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: false,
          }]);
      initNetworkConfig();
      networkConfig.shareNetwork_ = false;
      networkConfig.set('eapProperties_.outer', 'PEAP');
      await mojoApi_.whenCalled('getNetworkCertificates');
      await flushAsync();
      const outer = networkConfig.$$('#outer');
      assertEquals('PEAP', outer.value);
      // The first Server CA should be selected.
      assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
      // 'no-user-cert' is selected as user certificate because a
      // user certificate is not needed for PEAP
      assertEquals('no-user-cert', networkConfig.selectedUserCertHash_);
    });

    // Testing different WiFi EAP types with default Server CA certs.
    // Expected to see errors if both
    // [SerializedDomainSuffixMatch,  SerializedSubjectAltNameMatch] are empty.
    ['EAP-TLS', 'EAP-TTLS', 'PEAP'].forEach(eapType => {
      test('WiFi EAP Default CA Cert', async function() {
        const errorMessage = getErrorMessage(eapType);
        // No Server CA certificates provided, the default one will be selected.
        await initiateWiFiEapConfig(kServerCaCertsNotProvided, eapType);

        assertTrue(isDefaultServerCaSelected(), errorMessage);
        // Expected to see error because required fields are empty.
        assertTrue(isConfigErrorsPresent(), errorMessage);

        // Set an empty DomainSuffixMatch doesn't clear the error.
        await setSerializedDomainSuffixMatch('');
        assertTrue(isConfigErrorsPresent(), errorMessage);
        // Set an empty SerializedSubjectAltNameMatch doesn't clear
        // the error.
        await setSerializedSubjectAltNameMatch('');
        assertTrue(isMissedEapDataErrorShown(), errorMessage);

        // Set a non empty DomainSuffixMatch clears the error.
        await setSerializedDomainSuffixMatch('test.com');
        assertFalse(isConfigErrorsPresent(), errorMessage);
      });
    });

    // Testing that managed WiFi EAP networks which use the default Server CA
    // cert are not required to set a SerializedDomainSuffixMatch or
    // SerializedSubjectAltNameMatch.
    ['EAP-TLS', 'EAP-TTLS', 'PEAP'].forEach(eapType => {
      test('WiFi EAP Default CA Cert Managed EAP Settings', async function() {
        const errorMessage = getErrorMessage(eapType);
        mojoApi_.resetForTest();
        const wifi1 = OncMojo.getDefaultManagedProperties(
            NetworkType.kWiFi, 'testguid', '');
        const managed_eap = {
          outer: {activeValue: eapType},
          useSystemCas: {
            activeValue: true,
            policySource: PolicySource.kUserPolicyEnforced,
          },
        };
        wifi1.typeProperties.wifi.security = SecurityType.kWpaEap;
        wifi1.typeProperties.wifi.eap = managed_eap;

        setNetworkConfig(wifi1);
        initNetworkConfigWithCerts(
            /* hasServerCa= */ false, /* hasUserCert= */ true);
        await mojoApi_.whenCalled('getNetworkCertificates');
        await flushAsync();
        assertTrue(isDefaultServerCaSelected());
        assertFalse(isConfigErrorsPresent());
      });
    });

    // Testing LEAP with default Server CA certs. Expected to see
    // no errors because LEAP is not a certificate-based authentication
    // protocol.
    test('WiFi EAP Default CA Cert LEAP', async function() {
      const eapType = 'LEAP';

      // No Server CA certificates, the default one will be selected in UI.
      await initiateWiFiEapConfig(kServerCaCertsNotProvided, eapType);

      // 'default' Server CA should be selected.
      assertTrue(isDefaultServerCaSelected());
      assertFalse(isConfigErrorsPresent());

      // Setting an empty DomainSuffixMatch doesn't trigger errors.
      await setSerializedDomainSuffixMatch('');
      assertFalse(isConfigErrorsPresent());

      // Setting an empty SerializedSubjectAltNameMatch doesn't trigger errors.
      await setSerializedSubjectAltNameMatch('');
      assertFalse(isConfigErrorsPresent());

      // Setting non empty DomainSuffixMatch doesn't trigger errors.
      await setSerializedDomainSuffixMatch('test.com');
      assertFalse(isConfigErrorsPresent());

      // Setting non empty SerializedSubjectAltNameMatch doesn't trigger errors.
      await setSerializedDomainSuffixMatch('test.com');
      assertFalse(isConfigErrorsPresent());
    });

    // Testing different EAP auth methods with a non-default Server CA cert.
    // Expected to see no errors because empty [SerializedDomainSuffixMatch,
    // SerializedSubjectAltNameMatch] are allowed if a non-default (i.e.
    // non-public) Server CA is selected.
    ['EAP-TLS', 'EAP-TTLS', 'PEAP', 'LEAP'].forEach(eapType => {
      [true, false].forEach(isFeatureActive => {
        test('WiFi EAP Not Default CA Cert Flag', async function() {
          const errorMessage = getErrorMessage(eapType);

          // Adding Server CA certs, the first one will be selected by default.
          await initiateWiFiEapConfig(kAddServerCaCerts, eapType);

          // 1st Server CA should be selected and no errors.
          assertEquals(
              kCaHash, networkConfig.selectedServerCaHash_, errorMessage);
          assertFalse(isDefaultServerCaSelected(), errorMessage);
          assertFalse(isConfigErrorsPresent(), errorMessage);

          // Set an empty DomainSuffixMatch doesn't trigger errors.
          await setSerializedDomainSuffixMatch('');
          assertFalse(isConfigErrorsPresent(), errorMessage);

          // Set an empty SerializedSubjectAltNameMatch doesn't trigger errors.
          await setSerializedSubjectAltNameMatch('');
          assertFalse(isConfigErrorsPresent(), errorMessage);

          // Set non empty DomainSuffixMatch doesn't trigger errors.
          await setSerializedDomainSuffixMatch('test.com');
          assertFalse(isConfigErrorsPresent(), errorMessage);

          // Set non empty SerializedSubjectAltNameMatch doesn't trigger errors.
          await setSerializedDomainSuffixMatch('test.com');
          assertFalse(isConfigErrorsPresent(), errorMessage);
        });
      });
    });
  });
});
