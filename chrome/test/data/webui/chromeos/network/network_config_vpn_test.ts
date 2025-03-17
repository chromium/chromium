// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_config.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import type {NetworkConfigElement} from 'chrome://resources/ash/common/network/network_config.js';
import type {NetworkConfigInputElement} from 'chrome://resources/ash/common/network/network_config_input.js';
import type {NetworkConfigSelectElement} from 'chrome://resources/ash/common/network/network_config_select.js';
import type {NetworkPasswordInputElement} from 'chrome://resources/ash/common/network/network_password_input.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import type {ManagedIPConfigProperties, ManagedIPSecProperties, ManagedL2TPProperties, ManagedString, ManagedWireGuardProperties, NetworkCertificate} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeNetworkConfig} from '../fake_network_config_mojom.js';

import {clearBody, createNetworkConfigWithNetworkType, createNetworkConfigWithProperties} from './test_utils.js';

suite('network-config-vpn', function() {
  let networkConfig: NetworkConfigElement;

  let mojoApi_: FakeNetworkConfig;

  const kCaHash = 'CAHASH';
  const kUserHash1 = 'USERHASH1';
  const kCaPem = 'test-pem';
  const kUserCertId = 'test-cert-id';
  const kTestVpnName = 'test-vpn';
  const kTestVpnHost = 'test-vpn-host';
  const kTestUsername = 'test-username';
  const kTestPassword = 'test-password';
  const kTestPsk = 'test-psk';

  suiteSetup(function() {
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
            '#vpnServerCa');
    assertTrue(!!serverCa);
    assertTrue(typeof serverCa.value === 'string');
    return serverCa.value;
  }

  function getSelectedUserCertHashValue(): string {
    const userCert =
        networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
            '#vpnUserCert');
    assertTrue(!!userCert);
    assertTrue(typeof userCert.value === 'string');
    return userCert.value;
  }


  function getIpsecAuthType(): NetworkConfigSelectElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
        '#ipsec-auth-type');
  }

  function getL2tpUsernameInput(): NetworkConfigInputElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
        '#l2tp-username-input');
  }

  function getOpenVpnUsernameInput(): NetworkConfigInputElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
        '#openvpn-username-input');
  }

  function getVpnServerCa(): NetworkConfigSelectElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
        '#vpnServerCa');
  }

  function getVpnUserCert(): NetworkConfigSelectElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
        '#vpnUserCert');
  }

  function getWireguardIpInput(): NetworkConfigInputElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
        '#wireguard-ip-input');
  }

  function getWireguardPrivateKeyInput(): NetworkPasswordInputElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkPasswordInputElement>(
        '#wireguardPrivateKeyInput');
  }

  function getIpsecPskInput(): NetworkPasswordInputElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkPasswordInputElement>(
        '#ipsec-psk-input');
  }

  function getIpsecEapUsernameInput(): NetworkConfigInputElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
        '#ipsec-eap-username-input');
  }

  function getIpsecEapPasswordInput(): NetworkPasswordInputElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkPasswordInputElement>(
        '#ipsec-eap-password-input');
  }

  function getIpsecLocalIdInput(): NetworkConfigInputElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
        '#ipsec-local-id-input');
  }

  function getIpsecRemoteIdInput(): NetworkConfigInputElement|null {
    return networkConfig.shadowRoot!.querySelector<NetworkConfigInputElement>(
        '#ipsec-remote-id-input');
  }

  suite('OpenVPN', function() {
    setup(function() {
      mojoApi_.resetForTest();
      networkConfig = createNetworkConfigWithNetworkType(NetworkType.kVPN);
    });

    teardown(function() {
      clearBody();
    });

    test('Switch VPN Type', function() {
      initNetworkConfig();

      // Default VPN type is OpenVPN. Verify the displayed items.
      assertEquals('OpenVPN', networkConfig.get('vpnType_'));
      assertFalse(!!getIpsecAuthType());
      assertFalse(!!getL2tpUsernameInput());
      assertTrue(!!getOpenVpnUsernameInput());
      assertTrue(!!getVpnServerCa());
      assertTrue(!!getVpnUserCert());

      // Switch the VPN type to another and back again. Items should not change.
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      flush();
      networkConfig.set('vpnType_', 'OpenVPN');
      flush();
      assertFalse(!!getIpsecAuthType());
      assertFalse(!!getL2tpUsernameInput());
      assertTrue(!!getOpenVpnUsernameInput());
      assertTrue(!!getVpnServerCa());
      assertTrue(!!getVpnUserCert());
    });

    test('No Certs', function() {
      initNetworkConfig();
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          // Check that with no certificates, 'do-not-check' and 'no-user-certs'
          // are selected.
          assertEquals('do-not-check', getSelectedServerCaHashValue());
          assertEquals('no-user-cert', getSelectedUserCertHashValue());
        });
      });
    });

    test('Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          // The first Server CA should be selected.
          assertEquals(kCaHash, getSelectedServerCaHashValue());
          // OpenVPN allows but does not require a user certificate.
          assertEquals('no-user-cert', getSelectedUserCertHashValue());
        });
      });
    });
  });

  suite('WireGuard', function() {
    setup(function() {
      mojoApi_.resetForTest();
      networkConfig = createNetworkConfigWithNetworkType(NetworkType.kVPN);
      initNetworkConfig();
    });

    teardown(function() {
      clearBody();
    });

    test('Switch VPN Type', function() {
      const configProperties = networkConfig.get('configProperties_');
      networkConfig.set('vpnType_', 'OpenVPN');
      flush();
      assertFalse(!!configProperties.typeConfig.vpn.wireguard);
      assertFalse(!!getWireguardIpInput());
      networkConfig.set('vpnType_', 'WireGuard');
      flush();
      assertFalse(!!configProperties.typeConfig.vpn.openvpn);
      assertTrue(!!configProperties.typeConfig.vpn.wireguard);
      assertTrue(!!getWireguardIpInput());
    });

    test('Switch key config type', function() {
      networkConfig.set('vpnType_', 'WireGuard');
      flush();
      assertFalse(!!getWireguardPrivateKeyInput());
      networkConfig.set('wireguardKeyType_', 'UserInput');
      return flushTasks().then(() => {
        assertTrue(!!getWireguardPrivateKeyInput());
      });
    });

    test('Enable Connect', async function() {
      networkConfig.set('vpnType_', 'WireGuard');
      await flushTasks();
      assertFalse(networkConfig.enableConnect);

      networkConfig.set('ipAddressInput_', '10.10.0.1');
      const configProperties = networkConfig.get('configProperties_');
      configProperties.name = 'test-wireguard';
      const peer = configProperties.typeConfig.vpn.wireguard.peers[0];
      peer.publicKey = 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.publicKey`);
      await flushTasks();
      assertFalse(networkConfig.enableConnect);

      peer.endpoint = '192.168.66.66:32000';
      peer.allowedIps = '0.0.0.0/0';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.endpoint`);
      await flushTasks();
      assertTrue(networkConfig.enableConnect);

      peer.endpoint = '[fd01::1]:12345';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.endpoint`);
      await flushTasks();
      assertTrue(networkConfig.enableConnect);

      peer.presharedKey = 'invalid_key';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.presharedKey`);
      await flushTasks();
      assertFalse(networkConfig.enableConnect);

      peer.presharedKey = '';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.presharedKey`);
      await flushTasks();
      assertTrue(networkConfig.enableConnect);

      const badInputsForIp = [
        '10.10.0.1/32',
        '10.10.0.1,bad ip',
        '10.10.10.1,10.10.10.2',
        'fd00::1,fd00::2',
      ];
      for (const input of badInputsForIp) {
        networkConfig.set('ipAddressInput_', input);
        networkConfig.notifyPath(`configProperties_.ipAddressInput_`);
        await flushTasks();
        assertFalse(networkConfig.enableConnect);
      }

      const goodInputsForIp = ['10.10.0.1', 'fd00::1', '10.10.10.1,fd00::1'];
      for (const input of goodInputsForIp) {
        networkConfig.set('ipAddressInput_', input);
        networkConfig.notifyPath(`configProperties_.ipAddressInput_`);
        await flushTasks();
        assertTrue(networkConfig.enableConnect);
      }

      const badInputsForAllowedIps = ['0.0.0.0', '::', '0.0.0.0,::/0'];
      for (const input of badInputsForAllowedIps) {
        peer.allowedIps = input;
        networkConfig.notifyPath(
            `configProperties_.typeConfig.vpn.wireguard.peers.0.endpoint`);
        await flushTasks();
        assertFalse(networkConfig.enableConnect);
      }

      const goodInputsForAllowedIps = ['0.0.0.0/0', '::/0', '0.0.0.0/0,::/0'];
      for (const input of goodInputsForAllowedIps) {
        peer.allowedIps = input;
        networkConfig.notifyPath(
            `configProperties_.typeConfig.vpn.wireguard.peers.0.endpoint`);
        await flushTasks();
        assertTrue(networkConfig.enableConnect);
      }
    });
  });

  suite('Existing WireGuard', function() {
    setup(function() {
      mojoApi_.resetForTest();
      const wg1 =
          OncMojo.getDefaultManagedProperties(NetworkType.kVPN, 'someguid', '');
      assertTrue(!!wg1.typeProperties.vpn);
      wg1.typeProperties.vpn.type = VpnType.kWireGuard;
      const peers = {
        activeValue: [{
          publicKey: 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=',
          endpoint: '192.168.66.66:32000',
          allowedIps: '0.0.0.0/0,::/0',
          presharedKey: null,
          persistentKeepaliveInterval: 0,
        }],
        policySource: PolicySource.kNone,
        policyValue: null,
      };
      wg1.typeProperties.vpn.wireguard = {
        ipAddresses: {
          activeValue: ['10.10.0.1', 'fd00::1'],
          policySource: PolicySource.kNone,
          policyValue: null,
        },
        peers: peers,
        privateKey: null,
        publicKey: null,
      } as ManagedWireGuardProperties;
      const staticIpConfig = {
        nameServers: {activeValue: ['8.8.8.8', '8.8.4.4']},
      };
      wg1.staticIpConfig = staticIpConfig as ManagedIPConfigProperties;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, wg1);
      initNetworkConfig();
    });

    teardown(function() {
      clearBody();
    });

    test('Value Reflected', function() {
      return flushTasks().then(() => {
        const configProperties = networkConfig.get('configProperties_');
        const wireguardKeyType =
            networkConfig.shadowRoot!.querySelector<NetworkConfigSelectElement>(
                '#wireguard-key-type');
        assertTrue(!!wireguardKeyType);
        assertEquals('UseCurrent', wireguardKeyType.value);
        assertEquals('10.10.0.1,fd00::1', networkConfig.get('ipAddressInput_'));
        const peer = configProperties.typeConfig.vpn.wireguard.peers[0];
        assertEquals(
            'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=', peer.publicKey);
        assertEquals('192.168.66.66:32000', peer.endpoint);
        assertEquals('0.0.0.0/0,::/0', peer.allowedIps);
        assertEquals('8.8.8.8,8.8.4.4', networkConfig.get('nameServersInput_'));
      });
    });

    test('Preshared key display and config value', function() {
      return flushTasks().then(() => {
        const configProperties = networkConfig.get('configProperties_');
        assertEquals(
            '(credential)',
            configProperties.typeConfig.vpn.wireguard.peers[0].presharedKey);
        const configToSet = networkConfig.getPropertiesToSetForTesting();
        assertTrue(
            !!configToSet.typeConfig.vpn &&
            !!configToSet.typeConfig.vpn.wireguard &&
            !!configToSet.typeConfig.vpn.wireguard.peers);
        const peer = configToSet.typeConfig.vpn.wireguard.peers[0];
        assertTrue(!!peer);
        assertEquals(null, peer.presharedKey);
      });
    });
  });

  suite('IKEv2', function() {
    setup(function() {
      mojoApi_.resetForTest();
      networkConfig = createNetworkConfigWithNetworkType(NetworkType.kVPN);
    });

    teardown(function() {
      clearBody();
    });

    // Sets all mandatory fields for an IKEv2 VPN service.
    function setMandatoryFields() {
      const configProperties = networkConfig.get('configProperties_');
      configProperties.name = kTestVpnName;
      configProperties.typeConfig.vpn.host = kTestVpnHost;
    }

    // Checks that if fields are shown or hidden properly when switching
    // authentication type.
    test('Switch Authentication Type', function() {
      initNetworkConfig();

      networkConfig.set('vpnType_', 'IKEv2');
      flush();
      assertEquals(3, networkConfig.get('ipsecAuthTypeItems_').length);
      assertTrue(!!getIpsecAuthType());
      assertFalse(!!getL2tpUsernameInput());

      const ipsecAuthType = getIpsecAuthType();
      assertTrue(!!ipsecAuthType);
      assertEquals('EAP', ipsecAuthType.value);
      assertFalse(!!getIpsecPskInput());
      assertTrue(!!getVpnServerCa());
      assertFalse(!!getVpnUserCert());
      assertTrue(!!getIpsecEapUsernameInput());
      assertTrue(!!getIpsecEapPasswordInput());
      assertTrue(!!getIpsecLocalIdInput());
      assertTrue(!!getIpsecRemoteIdInput());

      networkConfig.set('ipsecAuthType_', 'PSK');
      flush();
      assertTrue(!!getIpsecPskInput());
      assertFalse(!!getVpnServerCa());
      assertFalse(!!getVpnUserCert());
      assertFalse(!!getIpsecEapUsernameInput());
      assertFalse(!!getIpsecEapPasswordInput());
      assertTrue(!!getIpsecLocalIdInput());
      assertTrue(!!getIpsecRemoteIdInput());

      networkConfig.set('ipsecAuthType_', 'Cert');
      flush();
      assertFalse(!!getIpsecPskInput());
      assertTrue(!!getVpnServerCa());
      assertTrue(!!getVpnUserCert());
      assertFalse(!!getIpsecEapUsernameInput());
      assertFalse(!!getIpsecEapPasswordInput());
      assertTrue(!!getIpsecLocalIdInput());
      assertTrue(!!getIpsecRemoteIdInput());
    });

    test('No Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ false, /* hasUserCert= */ false);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          assertEquals('no-certs', getSelectedServerCaHashValue());
          assertEquals('no-certs', getSelectedUserCertHashValue());

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA and user cert.
          setMandatoryFields();
          assertFalse(networkConfig.getVpnIsConfiguredForTesting());
        });
      });
    });

    test('No Server CA Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ false, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          assertEquals('no-certs', getSelectedServerCaHashValue());
          assertEquals(kUserHash1, getSelectedUserCertHashValue());

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA.
          setMandatoryFields();
          assertFalse(networkConfig.getVpnIsConfiguredForTesting());
        });
      });
    });

    test('No Client Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          assertEquals(kCaHash, getSelectedServerCaHashValue());
          assertEquals('no-certs', getSelectedUserCertHashValue());

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty client cert.
          setMandatoryFields();
          assertFalse(networkConfig.getVpnIsConfiguredForTesting());
        });
      });
    });

    // Checks if the values of vpnIsConfigured_() and getPropertiesToSet_() are
    // correct when the authentication type is PSK.
    test('PSK', function() {
      initNetworkConfig();
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'PSK');
      flush();

      setMandatoryFields();
      const configProperties = networkConfig.get('configProperties_');
      assertFalse(networkConfig.getVpnIsConfiguredForTesting());
      configProperties.typeConfig.vpn.ipSec.psk = kTestPsk;
      assertTrue(networkConfig.getVpnIsConfiguredForTesting());

      let props = networkConfig.getPropertiesToSetForTesting();
      assertEquals(kTestVpnName, props.name);
      assertTrue(!!props.typeConfig.vpn);
      assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
      assertTrue(!!props.typeConfig.vpn.type);
      assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
      assertTrue(!!props.typeConfig.vpn.ipSec);
      assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
      assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
      assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
      assertEquals(kTestPsk, props.typeConfig.vpn.ipSec.psk);
      assertEquals(null, props.typeConfig.vpn.ipSec.localIdentity);
      assertEquals(null, props.typeConfig.vpn.ipSec.remoteIdentity);

      networkConfig.set('vpnSaveCredentials_', true);
      props = networkConfig.getPropertiesToSetForTesting();
      assertTrue(!!props.typeConfig.vpn && !!props.typeConfig.vpn.ipSec);
      assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);

      configProperties.typeConfig.vpn.ipSec.localIdentity = 'local-id';
      configProperties.typeConfig.vpn.ipSec.remoteIdentity = 'remote-id';
      props = networkConfig.getPropertiesToSetForTesting();
      assertTrue(!!props.typeConfig.vpn && !!props.typeConfig.vpn.ipSec);
      assertEquals('local-id', props.typeConfig.vpn.ipSec.localIdentity);
      assertEquals('remote-id', props.typeConfig.vpn.ipSec.remoteIdentity);
    });

    // Checks if values are read correctly for an existing service of PSK
    // authentication.
    test('Existing PSK', function() {
      const ikev2 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'someguid', kTestVpnName);
      assertTrue(!!ikev2.typeProperties.vpn);
      ikev2.typeProperties.vpn.type = VpnType.kIKEv2;
      ikev2.typeProperties.vpn.host = {activeValue: kTestVpnHost} as
          ManagedString;
      const ipSec = {
        authenticationType: {activeValue: 'PSK'},
        ikeVersion: {activeValue: 2},
        localIdentity: {activeValue: 'local-id'},
        remoteIdentity: {activeValue: 'remote-id'},
        saveCredentials: {activeValue: true},
      };
      ikev2.typeProperties.vpn.ipSec = ipSec as ManagedIPSecProperties;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, ikev2);
      initNetworkConfig();

      return flushTasks().then(() => {
        assertEquals('IKEv2', networkConfig.get('vpnType_'));
        assertEquals('PSK', networkConfig.get('ipsecAuthType_'));

        // Populate the properties again. The values should be the same to what
        // are set above.
        const props = networkConfig.getPropertiesToSetForTesting();
        assertEquals('someguid', props.guid);
        assertEquals(kTestVpnName, props.name);
        assertTrue(!!props.typeConfig.vpn);
        assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
        assertTrue(!!props.typeConfig.vpn.type);
        assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
        assertTrue(!!props.typeConfig.vpn.ipSec);
        assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
        assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
        assertEquals('local-id', props.typeConfig.vpn.ipSec.localIdentity);
        assertEquals('remote-id', props.typeConfig.vpn.ipSec.remoteIdentity);
        assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
      });
    });

    // Checks if the values of vpnIsConfigured_() and getPropertiesToSet_() are
    // correct when the authentication type is user certificate.
    test('Cert', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          // The first Server CA and User certificate should be selected.
          assertEquals(kCaHash, getSelectedServerCaHashValue());
          assertEquals(kUserHash1, getSelectedUserCertHashValue());

          // Set all other mandatory fields. vpnIsConfigured_() should be true.
          setMandatoryFields();
          assertTrue(networkConfig.getVpnIsConfiguredForTesting());

          const props = networkConfig.getPropertiesToSetForTesting();
          assertEquals(kTestVpnName, props.name);
          assertTrue(!!props.typeConfig.vpn);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertTrue(!!props.typeConfig.vpn.type);
          assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertTrue(!!props.typeConfig.vpn.ipSec);
          assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertTrue(!!props.typeConfig.vpn.ipSec.serverCaPems);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
          assertEquals(
              kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
          assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
        });
      });
    });

    // Checks if values are read correctly for an existing service of
    // certificate authentication.
    test('Existing Cert', function() {
      const ikev2 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'someguid', kTestVpnName);
      assertTrue(!!ikev2.typeProperties.vpn);
      ikev2.typeProperties.vpn.type = VpnType.kIKEv2;
      ikev2.typeProperties.vpn.host = {activeValue: kTestVpnHost} as
          ManagedString;
      const ipSec = {
        authenticationType: {activeValue: 'Cert'},
        clientCertType: {activeValue: 'PKCS11Id'},
        clientCertPkcs11Id: {activeValue: kUserCertId},
        ikeVersion: {activeValue: 2},
        saveCredentials: {activeValue: true},
        serverCaPems: {activeValue: [kCaPem]},
      };
      ikev2.typeProperties.vpn.ipSec = ipSec as ManagedIPSecProperties;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, ikev2);
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          assertEquals('IKEv2', networkConfig.get('vpnType_'));
          assertEquals('Cert', networkConfig.get('ipsecAuthType_'));
          assertEquals(kCaHash, getSelectedServerCaHashValue());
          assertEquals(kUserHash1, getSelectedUserCertHashValue());

          const props = networkConfig.getPropertiesToSetForTesting();
          assertEquals('someguid', props.guid);
          assertEquals(kTestVpnName, props.name);
          assertTrue(!!props.typeConfig.vpn);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertTrue(!!props.typeConfig.vpn.type);
          assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertTrue(!!props.typeConfig.vpn.ipSec);
          assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertTrue(!!props.typeConfig.vpn.ipSec.serverCaPems);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
          assertEquals(
              kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
          assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
        });
      });
    });

    test('EAP', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'EAP');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          // Server CA should be selected.
          assertEquals(kCaHash, getSelectedServerCaHashValue());

          setMandatoryFields();
          assertFalse(networkConfig.getVpnIsConfiguredForTesting());
          const eapProperties = networkConfig.get('eapProperties_');
          eapProperties.identity = kTestUsername;
          eapProperties.password = kTestPassword;
          assertTrue(networkConfig.getVpnIsConfiguredForTesting());

          // Server CA is also mandatory when using EAP.
          networkConfig.set('selectedServerCaHash_', '');
          assertFalse(networkConfig.getVpnIsConfiguredForTesting());
          networkConfig.set('selectedServerCaHash_', kCaHash);

          let props = networkConfig.getPropertiesToSetForTesting();
          assertEquals(kTestVpnName, props.name);
          assertTrue(!!props.typeConfig.vpn);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertTrue(!!props.typeConfig.vpn.type);
          assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertTrue(!!props.typeConfig.vpn.ipSec);
          assertEquals('EAP', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertTrue(!!props.typeConfig.vpn.ipSec.serverCaPems);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertTrue(!!props.typeConfig.vpn.ipSec.eap);
          assertEquals('MSCHAPv2', props.typeConfig.vpn.ipSec.eap.outer);
          assertEquals(kTestUsername, props.typeConfig.vpn.ipSec.eap.identity);
          assertEquals(kTestPassword, props.typeConfig.vpn.ipSec.eap.password);
          assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
          assertFalse(props.typeConfig.vpn.ipSec.eap.saveCredentials);

          networkConfig.set('vpnSaveCredentials_', true);
          props = networkConfig.getPropertiesToSetForTesting();
          assertTrue(
              !!props.typeConfig.vpn && !!props.typeConfig.vpn.ipSec &&
              !!props.typeConfig.vpn.ipSec.eap);
          assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
          assertTrue(props.typeConfig.vpn.ipSec.eap.saveCredentials);
        });
      });
    });

    test('Existing EAP', function() {
      const ikev2 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'someguid', kTestVpnName);
      assertTrue(!!ikev2.typeProperties.vpn);
      ikev2.typeProperties.vpn.type = VpnType.kIKEv2;
      ikev2.typeProperties.vpn.host = {activeValue: kTestVpnHost} as
          ManagedString;
      const eap = {
        identity: {activeValue: kTestUsername},
        outer: {activeValue: 'MSCHAPv2'},
        saveCredentials: {activeValue: true},
        useSystemCas: {activeValue: false},
      };
      const ipSec = {
        authenticationType: {activeValue: 'EAP'},
        eap: eap,
        ikeVersion: {activeValue: 2},
        saveCredentials: {activeValue: true},
        serverCaPems: {activeValue: [kCaPem]},
      };
      ikev2.typeProperties.vpn.ipSec = ipSec as ManagedIPSecProperties;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, ikev2);
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          assertEquals('IKEv2', networkConfig.get('vpnType_'));
          assertEquals('EAP', networkConfig.get('ipsecAuthType_'));
          assertEquals(kCaHash, getSelectedServerCaHashValue());

          const props = networkConfig.getPropertiesToSetForTesting();
          assertEquals('someguid', props.guid);
          assertEquals(kTestVpnName, props.name);
          assertTrue(!!props.typeConfig.vpn);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertTrue(!!props.typeConfig.vpn.type);
          assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertTrue(!!props.typeConfig.vpn.ipSec);
          assertEquals('EAP', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertTrue(!!props.typeConfig.vpn.ipSec.serverCaPems);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertTrue(!!props.typeConfig.vpn.ipSec.eap);
          assertEquals('MSCHAPv2', props.typeConfig.vpn.ipSec.eap.outer);
          assertEquals(kTestUsername, props.typeConfig.vpn.ipSec.eap.identity);
          assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
          assertTrue(props.typeConfig.vpn.ipSec.eap.saveCredentials);
        });
      });
    });
  });

  suite('L2TP/IPsec', function() {
    setup(function() {
      mojoApi_.resetForTest();
      networkConfig = createNetworkConfigWithNetworkType(NetworkType.kVPN);
    });

    teardown(function() {
      clearBody();
    });

    // Sets all mandatory fields for an L2TP/IPsec service except for server CA
    // and user certificate.
    function setMandatoryFields(): void {
      const configProperties = networkConfig.get('configProperties_');
      configProperties.name = kTestVpnName;
      configProperties.typeConfig.vpn.host = kTestVpnHost;
      configProperties.typeConfig.vpn.l2tp.username = kTestUsername;
      configProperties.typeConfig.vpn.l2tp.password = kTestPassword;
    }

    test('Switch Authentication Type', function() {
      initNetworkConfig();

      // Switch to L2TP/IPsec, the authentication type is default to PSK. The
      // PSK input should appear and the dropdowns for server CA and user
      // certificate should be hidden.
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      flush();
      assertEquals(2, networkConfig.get('ipsecAuthTypeItems_').length);
      let ipsecAuthType = getIpsecAuthType();
      assertTrue(!!ipsecAuthType);
      assertEquals('PSK', ipsecAuthType.value);
      assertFalse(!!getIpsecLocalIdInput());
      assertFalse(!!getIpsecRemoteIdInput());
      assertTrue(!!getIpsecAuthType());
      assertTrue(!!getL2tpUsernameInput());
      assertTrue(!!getIpsecPskInput());
      assertFalse(!!getVpnServerCa());
      assertFalse(!!getVpnUserCert());

      // Switch the authentication type to Cert. The PSK input should be hidden
      // and the dropdowns for server CA and user certificate should appear.
      networkConfig.set('ipsecAuthType_', 'Cert');
      flush();
      assertFalse(!!getIpsecPskInput());
      assertTrue(!!getIpsecAuthType());
      assertTrue(!!getL2tpUsernameInput());
      assertTrue(!!getVpnServerCa());
      assertTrue(!!getVpnUserCert());

      // Switch VPN type to IKEv2 and auth type to EAP, and then back to
      // L2TP/IPsec. The auth type should be reset to PSK since EAP is not a
      // valid value.
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'EAP');
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      ipsecAuthType = getIpsecAuthType();
      assertTrue(!!ipsecAuthType);
      assertEquals('PSK', ipsecAuthType.value);
    });

    test('No Certs', function() {
      initNetworkConfig();
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          // Check that with no certificates, 'do-not-check' and 'no-certs' are
          // selected.
          assertEquals('no-certs', getSelectedServerCaHashValue());
          assertEquals('no-certs', getSelectedUserCertHashValue());

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA and user cert.
          setMandatoryFields();
          assertFalse(networkConfig.getVpnIsConfiguredForTesting());
        });
      });
    });

    test('No Server CA Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ false, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          assertEquals('no-certs', getSelectedServerCaHashValue());
          assertEquals(kUserHash1, getSelectedUserCertHashValue());

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA.
          setMandatoryFields();
          assertFalse(networkConfig.getVpnIsConfiguredForTesting());
        });
      });
    });

    test('No Client Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      initNetworkConfig();
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          assertEquals(kCaHash, getSelectedServerCaHashValue());
          assertEquals('no-certs', getSelectedUserCertHashValue());

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty client cert.
          setMandatoryFields();
          assertFalse(networkConfig.getVpnIsConfiguredForTesting());
        });
      });
    });

    // Checks if the values of vpnIsConfigured_() and getPropertiesToSet_() are
    // correct when the authentication type is PSK.
    test('PSK', function() {
      initNetworkConfig();
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      flush();

      setMandatoryFields();
      const configProperties = networkConfig.get('configProperties_');
      assertFalse(networkConfig.getVpnIsConfiguredForTesting());
      configProperties.typeConfig.vpn.ipSec.psk = kTestPsk;
      assertTrue(networkConfig.getVpnIsConfiguredForTesting());

      let props = networkConfig.getPropertiesToSetForTesting();
      assertEquals(kTestVpnName, props.name);
      assertTrue(!!props.typeConfig.vpn);
      assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
      assertTrue(!!props.typeConfig.vpn.type);
      assertEquals(VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
      assertTrue(!!props.typeConfig.vpn.ipSec);
      assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
      assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
      assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
      assertEquals(kTestPsk, props.typeConfig.vpn.ipSec.psk);
      assertTrue(!!props.typeConfig.vpn.l2tp);
      assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
      assertEquals(kTestPassword, props.typeConfig.vpn.l2tp.password);

      networkConfig.set('vpnSaveCredentials_', true);
      props = networkConfig.getPropertiesToSetForTesting();
      assertTrue(!!props.typeConfig.vpn && !!props.typeConfig.vpn.ipSec);
      assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
      assertTrue(!!props.typeConfig.vpn.l2tp);
      assertTrue(props.typeConfig.vpn.l2tp.saveCredentials);
    });

    // Checks if values are read correctly for an existing service of PSK
    // authentication.
    test('Existing PSK', function() {
      const l2tp = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'someguid', kTestVpnName);
      assertTrue(!!l2tp.typeProperties.vpn);
      l2tp.typeProperties.vpn.type = VpnType.kL2TPIPsec;
      l2tp.typeProperties.vpn.host = {activeValue: kTestVpnHost} as
          ManagedString;
      const ipSec = {
        authenticationType: {activeValue: 'PSK'},
        ikeVersion: {activeValue: 1},
        saveCredentials: {activeValue: true},
      };
      l2tp.typeProperties.vpn.ipSec = ipSec as ManagedIPSecProperties;
      const vpnL2tp = {
        username: {activeValue: kTestUsername},
        saveCredentials: {activeValue: true},
      };
      l2tp.typeProperties.vpn.l2tp = vpnL2tp as ManagedL2TPProperties;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, l2tp);
      initNetworkConfig();

      return flushTasks().then(() => {
        assertEquals('L2TP_IPsec', networkConfig.get('vpnType_'));
        assertEquals('PSK', networkConfig.get('ipsecAuthType_'));

        // Populate the properties again. The values should be the same to what
        // are set above.
        const props = networkConfig.getPropertiesToSetForTesting();
        assertEquals('someguid', props.guid);
        assertEquals(kTestVpnName, props.name);
        assertTrue(!!props.typeConfig.vpn);
        assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
        assertTrue(!!props.typeConfig.vpn.type);
        assertEquals(VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
        assertTrue(!!props.typeConfig.vpn.ipSec);
        assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
        assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
        assertEquals(null, props.typeConfig.vpn.ipSec.eap);
        assertEquals(null, props.typeConfig.vpn.ipSec.localIdentity);
        assertEquals(null, props.typeConfig.vpn.ipSec.remoteIdentity);
        assertTrue(!!props.typeConfig.vpn.l2tp);
        assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
        assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
        assertTrue(props.typeConfig.vpn.l2tp.saveCredentials);
      });
    });

    // Checks if the values of vpnIsConfigured_() and getPropertiesToSet_() are
    // correct when the authentication type is user certificate.
    test('Cert', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushTasks().then(() => {
          // The first Server CA and User certificate should be selected.
          assertEquals(kCaHash, getSelectedServerCaHashValue());
          assertEquals(kUserHash1, getSelectedUserCertHashValue());

          // Set all other mandatory fields. vpnIsConfigured_() should be true.
          setMandatoryFields();
          assertTrue(networkConfig.getVpnIsConfiguredForTesting());

          const props = networkConfig.getPropertiesToSetForTesting();
          assertEquals(kTestVpnName, props.name);
          assertTrue(!!props.typeConfig.vpn);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertTrue(!!props.typeConfig.vpn.type);
          assertEquals(VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
          assertTrue(!!props.typeConfig.vpn.ipSec);
          assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
          assertTrue(!!props.typeConfig.vpn.ipSec.serverCaPems);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
          assertEquals(
              kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
          assertTrue(!!props.typeConfig.vpn.l2tp);
          assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
          assertEquals(kTestPassword, props.typeConfig.vpn.l2tp.password);
          assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
          assertFalse(props.typeConfig.vpn.l2tp.saveCredentials);
        });
      });
    });

    // Checks if values are read correctly for an existing service of
    // certificate authentication.
    test('Existing Cert', function() {
      const l2tp = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'someguid', kTestVpnName);
      assertTrue(!!l2tp.typeProperties.vpn);
      l2tp.typeProperties.vpn.type = VpnType.kL2TPIPsec;
      l2tp.typeProperties.vpn.host = {activeValue: kTestVpnHost} as
          ManagedString;
      const ipSec = {
        authenticationType: {activeValue: 'Cert'},
        clientCertType: {activeValue: 'PKCS11Id'},
        clientCertPkcs11Id: {activeValue: kUserCertId},
        ikeVersion: {activeValue: 1},
        saveCredentials: {activeValue: true},
        serverCaPems: {activeValue: [kCaPem]},
      };
      l2tp.typeProperties.vpn.ipSec = ipSec as ManagedIPSecProperties;
      const vpnL2tp = {
        username: {activeValue: kTestUsername},
        saveCredentials: {activeValue: true},
      };
      l2tp.typeProperties.vpn.l2tp = vpnL2tp as ManagedL2TPProperties;
      networkConfig = createNetworkConfigWithProperties(mojoApi_, l2tp);
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        assertEquals('L2TP_IPsec', networkConfig.get('vpnType_'));
        assertEquals('Cert', networkConfig.get('ipsecAuthType_'));
        assertEquals(kCaHash, getSelectedServerCaHashValue());
        assertEquals(kUserHash1, getSelectedUserCertHashValue());

        // Populate the properties again. The values should be the same to what
        // are set above.
        const props = networkConfig.getPropertiesToSetForTesting();
        assertEquals('someguid', props.guid);
        assertEquals(kTestVpnName, props.name);
        assertTrue(!!props.typeConfig.vpn);
        assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
        assertTrue(!!props.typeConfig.vpn.type);
        assertEquals(VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
        assertTrue(!!props.typeConfig.vpn.ipSec);
        assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
        assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
        assertTrue(!!props.typeConfig.vpn.ipSec.serverCaPems);
        assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
        assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
        assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
        assertEquals(
            kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
        assertEquals(null, props.typeConfig.vpn.ipSec.eap);
        assertEquals(null, props.typeConfig.vpn.ipSec.localIdentity);
        assertEquals(null, props.typeConfig.vpn.ipSec.remoteIdentity);
        assertTrue(!!props.typeConfig.vpn.l2tp);
        assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
        assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
        assertTrue(props.typeConfig.vpn.l2tp.saveCredentials);
      });
    });
  });
});
