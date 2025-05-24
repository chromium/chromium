// Copyright 2023 The Chromium Authors
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
import type {ManagedEAPProperties, ManagedString, NetworkCertificate} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {HiddenSsidMode, SecurityType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, OncSource, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

import {clearBody, createNetworkConfigWithNetworkType, createNetworkConfigWithProperties, simulateEnterPressedInElement} from './test_utils.js';

suite('network-config-wifi', () => {
  let networkConfig: NetworkConfigElement;

  let mojoApi_: FakeNetworkConfig;

  const kCaHash = 'CAHASH';
  const kCaPem = 'test-pem';
  const kUserHash1 = 'USERHASH1';
  const kUserHash2 = 'USERHASH2';
  const kUserCertId = 'test-cert-id';
  const kMissedEapDataErr = 'missingEapDefaultServerCaSubjectVerification';
  const kServerCaCertsNotProvided = false;
  const kAddServerCaCerts = true;

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

  function initNetworkConfigWithCerts(
      hasServerCa: boolean, hasUserCert: boolean): void {
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
    mojoApi_.setCertificatesForTest(
        serverCas as NetworkCertificate[], userCerts as NetworkCertificate[]);
    initNetworkConfig();
  }

  function getSelectedServerCaHashValue(): string {
    const serverCa =
        networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
            '#serverCa');
    assertTrue(!!serverCa);
    assertTrue(typeof serverCa.value === 'string');
    return serverCa.value;
  }

  function getSelectedUserCertHashValue(): string {
    const userCert =
        networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
            '#userCert');
    assertTrue(!!userCert);
    assertTrue(typeof userCert.value === 'string');
    return userCert.value;
  }

  suite('New WiFi Config', () => {
    setup(() => {
      mojoApi_.resetForTest();
      networkConfig = createNetworkConfigWithNetworkType(NetworkType.kWiFi);
      initNetworkConfig();
    });

    teardown(() => {
      clearBody();
    });

    test('Default', () => {
      assertTrue(
          !!networkConfig.shadowRoot!.querySelector<NetworkConfigToggleElement>(
              '#share'));
      assertTrue(
          !!networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
              '#ssid'));
      const security =
          networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
              '#security');
      assertTrue(!!security);
      assertFalse(security.disabled);
    });

    test('Passphrase field shows', () => {
      assertFalse(!!networkConfig.shadowRoot!
                        .querySelector<NetworkPasswordInputElement>(
                            '#wifi-passphrase'));
      const security =
          networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
              '#security');
      assertTrue(!!security);
      security.value = SecurityType.kWpaPsk;
      return flushTasks().then(() => {
        assertTrue(!!networkConfig.shadowRoot!
                         .querySelector<NetworkPasswordInputElement>(
                             '#wifi-passphrase'));
      });
    });

    test('New networks are explicitly not hidden', async () => {
      networkConfig.save();

      await flushTasks();

      const props = mojoApi_.getPropertiesToSetForTest();
      assertTrue(!!props?.typeConfig.wifi);
      assertEquals(props.typeConfig.wifi.hiddenSsid, HiddenSsidMode.kDisabled);
    });
  });

  suite('Existing WiFi Config', () => {
    setup(() => {
      mojoApi_.resetForTest();
      const wifi1 = OncMojo.getDefaultManagedProperties(
          NetworkType.kWiFi, 'someguid', '');
      wifi1.name = OncMojo.createManagedString('somename');
      wifi1.source = OncSource.kDevice;
      assertTrue(!!wifi1.typeProperties.wifi);
      wifi1.typeProperties.wifi.security = SecurityType.kWepPsk;
      wifi1.typeProperties.wifi.ssid.activeValue = '11111111111';
      wifi1.typeProperties.wifi.passphrase = {activeValue: 'test_passphrase'} as
          ManagedString;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, wifi1);
      initNetworkConfig();
    });

    teardown(() => {
      clearBody();
    });

    test('Default', () => {
      return flushTasks().then(() => {
        assertEquals(
            'someguid', networkConfig.getManagedPropertiesForTesting().guid);
        const name = networkConfig.getManagedPropertiesForTesting().name;
        assertEquals('somename', name?.activeValue);
        assertFalse(!!networkConfig.shadowRoot!
                          .querySelector<NetworkConfigToggleElement>('#share'));
        assertTrue(!!networkConfig.shadowRoot!
                         .querySelector<NetworkConfigInputElement>('#ssid'));
        assertTrue(
            !!networkConfig.shadowRoot!
                  .querySelector<NetworkConfigSelectElement>('#security'));
        const security =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#security');
        assertTrue(!!security);
        assertTrue(security.disabled);
      });
    });

    test('WiFi input fires enter event on keydown', () => {
      return flushTasks().then(() => {
        assertFalse(networkConfig.getPropertiesSentForTesting());
        simulateEnterPressedInElement(networkConfig, 'ssid');
        assertTrue(networkConfig.getPropertiesSentForTesting());
      });
    });

    test('Remove error text when input key is pressed', () => {
      return flushTasks().then(() => {
        networkConfig.error = 'bad-passphrase';
        const passwordInput =
            networkConfig.shadowRoot!
                .querySelector<NetworkPasswordInputElement>('#wifi-passphrase');
        assertTrue(!!passwordInput);

        passwordInput.dispatchEvent(new Event('keypress'));
        flush();
        assertFalse(!!networkConfig.error);
      });
    });

    test('Networks\' hidden SSID mode is not overwritten', async () => {
      await flushTasks();

      networkConfig.save();

      await flushTasks();

      const props = mojoApi_.getPropertiesToSetForTest();
      assertTrue(!!props?.typeConfig.wifi);
      assertEquals(props.typeConfig.wifi.hiddenSsid, HiddenSsidMode.kAutomatic);
    });
  });

  suite('Non-VPN EAP', () => {
    setup(() => {
      mojoApi_.resetForTest();
    });

    teardown(() => {
      clearBody();
    });

    function setAuthenticated(): void {
      // Logged in users can share new networks.
      networkConfig.shareAllowEnable = true;
      // Authenticated networks default to not shared.
      networkConfig.shareDefault = false;
    }

    async function saveNetworkConfig(): Promise<void> {
      networkConfig.save();
      await flushTasks();
    }

    async function setSerializedSubjectAltNameMatch(value: string):
        Promise<void> {
      networkConfig.setSerializedSubjectAltNameMatchForTesting(value);
      await saveNetworkConfig();
    }

    async function setSerializedDomainSuffixMatch(value: string):
        Promise<void> {
      networkConfig.setSerializedDomainSuffixMatchForTesting(value);
      await saveNetworkConfig();
    }

    function isDefaultServerCaSelected(): boolean {
      return getSelectedServerCaHashValue() === 'default';
    }

    function isConfigErrorsPresent(): boolean {
      return networkConfig.error !== '';
    }

    function isMissedEapDataErrorShown(): boolean {
      return kMissedEapDataErr === networkConfig.error;
    }

    function getErrorMessage(eapType: string): string {
      return 'Failed test for eapType = ' + eapType;
    }

    async function initiateWiFiEapConfig(
        isAddServerCA: boolean, eapType: string): Promise<void> {
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfigWithCerts(isAddServerCA, /* hasUserCert= */ true);
      networkConfig.setShareNetworkForTesting(/* shareNetwork= */ true);
      networkConfig.set('eapProperties_.outer', eapType);
      await mojoApi_.whenCalled('getNetworkCertificates');
      networkConfig.save();
      await flushTasks();
    }

    test('WiFi EAP Default Outer', async () => {
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfig();
      networkConfig.setShareNetworkForTesting(/* shareNetwork= */ false);
      await mojoApi_.whenCalled('getNetworkCertificates');
      await flushTasks();
      const outer =
          networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
              '#outer');
      // 'PEAP' should be the default 'Outer' protocol.
      assertEquals('PEAP', outer?.value);
    });

    test('WiFi EAP-TLS No Certs', () => {
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfig();
      networkConfig.setShareNetworkForTesting(/* shareNetwork= */ false);
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          const outer =
              networkConfig.shadowRoot!
                  .querySelector<NetworkConfigSelectElement>('#outer');
          assertEquals('EAP-TLS', outer?.value);
          // Check that with no certificates, 'default' and 'no-certs' are
          // selected.
          assertEquals('default', getSelectedServerCaHashValue());
          assertEquals('no-certs', getSelectedUserCertHashValue());
        });
      });
    });

    test('WiFi EAP-TLS Certs', () => {
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true,
          } as NetworkCertificate],
          [{
            hash: kUserHash1,
            pemOrId: kUserCertId,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: false,
          } as NetworkCertificate]);
      initNetworkConfig();
      networkConfig.setShareNetworkForTesting(/* shareNetwork= */ false);
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          // The first Server CA  and User certificate should be selected.
          assertEquals(kCaHash, getSelectedServerCaHashValue());
          assertEquals(kUserHash1, getSelectedUserCertHashValue());
        });
      });
    });

    test('WiFi EAP-TLS Certs Shared', () => {
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true,
          } as NetworkCertificate],
          [
            {
              hash: kUserHash1,
              pemOrId: kUserCertId,
              availableForNetworkAuth: true,
              hardwareBacked: true,
              deviceWide: false,
            } as NetworkCertificate,
            {
              hash: kUserHash2,
              pemOrId: kUserCertId,
              availableForNetworkAuth: true,
              hardwareBacked: true,
              deviceWide: true,
            } as NetworkCertificate,
          ]);
      initNetworkConfig();
      networkConfig.setShareNetworkForTesting(/* shareNetwork= */ true);
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          // The first Server CA should be selected.
          assertEquals(kCaHash, getSelectedServerCaHashValue());
          // Second User Hash should be selected since it is a device cert.
          assertEquals(kUserHash2, getSelectedUserCertHashValue());
        });
      });
    });

    test('WiFi PEAP No Certs', async () => {
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfig();
      networkConfig.setShareNetworkForTesting(/* shareNetwork= */ false);
      networkConfig.set('eapProperties_.outer', 'PEAP');
      await mojoApi_.whenCalled('getNetworkCertificates');
      await flushTasks();
      const outer =
          networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
              '#outer');
      assertEquals('PEAP', outer?.value);
      // 'default' Server CA should be selected in case of no certificates
      assertEquals('default', getSelectedServerCaHashValue());
      // 'no-user-cert' is selected as user certificate because a
      // user certificate is not needed for PEAP
      assertEquals('no-user-cert', getSelectedUserCertHashValue());
    });

    test('WiFi PEAP Certs', async () => {
      networkConfig = createNetworkConfigWithNetworkType(
          NetworkType.kWiFi, SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true,
          } as NetworkCertificate],
          [{
            hash: kUserHash1,
            pemOrId: kUserCertId,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: false,
          } as NetworkCertificate]);
      initNetworkConfig();
      networkConfig.setShareNetworkForTesting(/* shareNetwork= */ false);
      networkConfig.set('eapProperties_.outer', 'PEAP');
      await mojoApi_.whenCalled('getNetworkCertificates');
      await flushTasks();
      const outer =
          networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
              '#outer');
      assertEquals('PEAP', outer?.value);
      // The first Server CA should be selected.
      assertEquals(kCaHash, getSelectedServerCaHashValue());
      // 'no-user-cert' is selected as user certificate because a
      // user certificate is not needed for PEAP
      assertEquals('no-user-cert', getSelectedUserCertHashValue());
    });

    // Testing different WiFi EAP types with default Server CA certs.
    // Expected to see errors if both
    // [SerializedDomainSuffixMatch,  SerializedSubjectAltNameMatch] are empty.
    ['EAP-TLS', 'EAP-TTLS', 'PEAP'].forEach(eapType => {
      test('WiFi EAP Default CA Cert', async () => {
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
      test('WiFi EAP Default CA Cert Managed EAP Settings', async () => {
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
        assertTrue(!!wifi1.typeProperties.wifi);
        wifi1.typeProperties.wifi.security = SecurityType.kWpaEap;
        wifi1.typeProperties.wifi.eap = managed_eap as ManagedEAPProperties;

        networkConfig = createNetworkConfigWithProperties(mojoApi_, wifi1);
        initNetworkConfigWithCerts(
            /* hasServerCa= */ false, /* hasUserCert= */ true);
        await mojoApi_.whenCalled('getNetworkCertificates');
        await flushTasks();
        assertTrue(isDefaultServerCaSelected());
        assertFalse(isConfigErrorsPresent());
      });
    });

    // Testing LEAP with default Server CA certs. Expected to see
    // no errors because LEAP is not a certificate-based authentication
    // protocol.
    test('WiFi EAP Default CA Cert LEAP', async () => {
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
      test('WiFi EAP Not Default CA Cert Flag', async () => {
        const errorMessage = getErrorMessage(eapType);

        // Adding Server CA certs, the first one will be selected by default.
        await initiateWiFiEapConfig(kAddServerCaCerts, eapType);

        // 1st Server CA should be selected and no errors.
        assertEquals(kCaHash, getSelectedServerCaHashValue(), errorMessage);
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
