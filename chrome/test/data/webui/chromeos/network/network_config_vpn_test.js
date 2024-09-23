// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_config.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfigRemote, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';

suite('network-config-vpn', function() {
  let networkConfig;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

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

  suite('OpenVPN', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(NetworkType.kVPN);
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Switch VPN Type', function() {
      initNetworkConfig();

      // Default VPN type is OpenVPN. Verify the displayed items.
      assertEquals('OpenVPN', networkConfig.get('vpnType_'));
      assertFalse(!!networkConfig.$$('#ipsec-auth-type'));
      assertFalse(!!networkConfig.$$('#l2tp-username-input'));
      assertTrue(!!networkConfig.$$('#openvpn-username-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertTrue(!!networkConfig.$$('#vpnUserCert'));

      // Switch the VPN type to another and back again. Items should not change.
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      flush();
      networkConfig.set('vpnType_', 'OpenVPN');
      flush();
      assertFalse(!!networkConfig.$$('#ipsec-auth-type'));
      assertFalse(!!networkConfig.$$('#l2tp-username-input'));
      assertTrue(!!networkConfig.$$('#openvpn-username-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertTrue(!!networkConfig.$$('#vpnUserCert'));
    });

    test('No Certs', function() {
      initNetworkConfig();
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // Check that with no certificates, 'do-not-check' and 'no-user-certs'
          // are selected.
          assertEquals('do-not-check', networkConfig.selectedServerCaHash_);
          assertEquals('no-user-cert', networkConfig.selectedUserCertHash_);
        });
      });
    });

    test('Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // The first Server CA should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          // OpenVPN allows but does not require a user certificate.
          assertEquals('no-user-cert', networkConfig.selectedUserCertHash_);
        });
      });
    });
  });

  suite('WireGuard', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(NetworkType.kVPN);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Switch VPN Type', function() {
      const configProperties = networkConfig.get('configProperties_');
      networkConfig.set('vpnType_', 'OpenVPN');
      flush();
      assertFalse(!!configProperties.typeConfig.vpn.wireguard);
      assertFalse(!!networkConfig.$$('#wireguard-ip-input'));
      networkConfig.set('vpnType_', 'WireGuard');
      flush();
      assertFalse(!!configProperties.typeConfig.vpn.openvpn);
      assertTrue(!!configProperties.typeConfig.vpn.wireguard);
      assertTrue(!!networkConfig.$$('#wireguard-ip-input'));
    });

    test('Switch key config type', function() {
      networkConfig.set('vpnType_', 'WireGuard');
      flush();
      assertFalse(!!networkConfig.$$('#wireguardPrivateKeyInput'));
      networkConfig.set('wireguardKeyType_', 'UserInput');
      return flushAsync().then(() => {
        assertTrue(!!networkConfig.$$('#wireguardPrivateKeyInput'));
      });
    });

    test('Enable Connect', async function() {
      networkConfig.set('vpnType_', 'WireGuard');
      await flushAsync();
      assertFalse(networkConfig.enableConnect);

      networkConfig.set('ipAddressInput_', '10.10.0.1');
      const configProperties = networkConfig.get('configProperties_');
      configProperties.name = 'test-wireguard';
      const peer = configProperties.typeConfig.vpn.wireguard.peers[0];
      peer.publicKey = 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.publicKey`);
      await flushAsync();
      assertFalse(networkConfig.enableConnect);

      peer.endpoint = '192.168.66.66:32000';
      peer.allowedIps = '0.0.0.0/0';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.endpoint`);
      await flushAsync();
      assertTrue(networkConfig.enableConnect);

      peer.endpoint = '[fd01::1]:12345';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.endpoint`);
      await flushAsync();
      assertTrue(networkConfig.enableConnect);

      peer.presharedKey = 'invalid_key';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.presharedKey`);
      await flushAsync();
      assertFalse(networkConfig.enableConnect);

      peer.presharedKey = '';
      networkConfig.notifyPath(
          `configProperties_.typeConfig.vpn.wireguard.peers.0.presharedKey`);
      await flushAsync();
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
        await flushAsync();
        assertFalse(networkConfig.enableConnect);
      }

      const goodInputsForIp = ['10.10.0.1', 'fd00::1', '10.10.10.1,fd00::1'];
      for (const input of goodInputsForIp) {
        networkConfig.set('ipAddressInput_', input);
        networkConfig.notifyPath(`configProperties_.ipAddressInput_`);
        await flushAsync();
        assertTrue(networkConfig.enableConnect);
      }

      const badInputsForAllowedIps = ['0.0.0.0', '::', '0.0.0.0,::/0'];
      for (const input of badInputsForAllowedIps) {
        peer.allowedIps = input;
        networkConfig.notifyPath(
            `configProperties_.typeConfig.vpn.wireguard.peers.0.endpoint`);
        await flushAsync();
        assertFalse(networkConfig.enableConnect);
      }

      const goodInputsForAllowedIps = ['0.0.0.0/0', '::/0', '0.0.0.0/0,::/0'];
      for (const input of goodInputsForAllowedIps) {
        peer.allowedIps = input;
        networkConfig.notifyPath(
            `configProperties_.typeConfig.vpn.wireguard.peers.0.endpoint`);
        await flushAsync();
        assertTrue(networkConfig.enableConnect);
      }
    });
  });

  suite('Existing WireGuard', function() {
    setup(function() {
      mojoApi_.resetForTest();
      const wg1 =
          OncMojo.getDefaultManagedProperties(NetworkType.kVPN, 'someguid', '');
      wg1.typeProperties.vpn.type = VpnType.kWireGuard;
      wg1.typeProperties.vpn.wireguard = {
        ipAddresses: {activeValue: ['10.10.0.1', 'fd00::1']},
        peers: {
          activeValue: [{
            publicKey: 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=',
            endpoint: '192.168.66.66:32000',
            allowedIps: '0.0.0.0/0,::/0',
          }],
        },
      };
      wg1.staticIpConfig = {nameServers: {activeValue: ['8.8.8.8', '8.8.4.4']}};
      setNetworkConfig(wg1);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Value Reflected', function() {
      return flushAsync().then(() => {
        const configProperties = networkConfig.get('configProperties_');
        const peer = configProperties.typeConfig.vpn.wireguard.peers[0];
        assertEquals('UseCurrent', networkConfig.wireguardKeyType_);
        assertEquals('10.10.0.1,fd00::1', networkConfig.get('ipAddressInput_'));
        assertEquals(
            'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=', peer.publicKey);
        assertEquals('192.168.66.66:32000', peer.endpoint);
        assertEquals('0.0.0.0/0,::/0', peer.allowedIps);
        assertEquals('8.8.8.8,8.8.4.4', networkConfig.get('nameServersInput_'));
      });
    });

    test('Preshared key display and config value', function() {
      return flushAsync().then(() => {
        const configProperties = networkConfig.get('configProperties_');
        assertEquals(
            '(credential)',
            configProperties.typeConfig.vpn.wireguard.peers[0].presharedKey);
        const configToSet = networkConfig.getPropertiesToSet_();
        assertEquals(
            undefined,
            configToSet.typeConfig.vpn.wireguard.peers[0].presharedKey);
      });
    });
  });

  suite('IKEv2', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(NetworkType.kVPN);
    });

    teardown(function() {
      PolymerTest.clearBody();
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
      assertTrue(!!networkConfig.$$('#ipsec-auth-type'));
      assertFalse(!!networkConfig.$$('#l2tp-username-input'));

      assertEquals('EAP', networkConfig.ipsecAuthType_);
      assertFalse(!!networkConfig.$$('#ipsec-psk-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertFalse(!!networkConfig.$$('#vpnUserCert'));
      assertTrue(!!networkConfig.$$('#ipsec-eap-username-input'));
      assertTrue(!!networkConfig.$$('#ipsec-eap-password-input'));
      assertTrue(!!networkConfig.$$('#ipsec-local-id-input'));
      assertTrue(!!networkConfig.$$('#ipsec-remote-id-input'));

      networkConfig.set('ipsecAuthType_', 'PSK');
      flush();
      assertTrue(!!networkConfig.$$('#ipsec-psk-input'));
      assertFalse(!!networkConfig.$$('#vpnServerCa'));
      assertFalse(!!networkConfig.$$('#vpnUserCert'));
      assertFalse(!!networkConfig.$$('#ipsec-eap-username-input'));
      assertFalse(!!networkConfig.$$('#ipsec-eap-password-input'));
      assertTrue(!!networkConfig.$$('#ipsec-local-id-input'));
      assertTrue(!!networkConfig.$$('#ipsec-remote-id-input'));

      networkConfig.set('ipsecAuthType_', 'Cert');
      flush();
      assertFalse(!!networkConfig.$$('#ipsec-psk-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertTrue(!!networkConfig.$$('#vpnUserCert'));
      assertFalse(!!networkConfig.$$('#ipsec-eap-username-input'));
      assertFalse(!!networkConfig.$$('#ipsec-eap-password-input'));
      assertTrue(!!networkConfig.$$('#ipsec-local-id-input'));
      assertTrue(!!networkConfig.$$('#ipsec-remote-id-input'));
    });

    test('No Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ false, /* hasUserCert= */ false);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('no-certs', networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA and user cert.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
        });
      });
    });

    test('No Server CA Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ false, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('no-certs', networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
        });
      });
    });

    test('No Client Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty client cert.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
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
      assertFalse(networkConfig.vpnIsConfigured_());
      configProperties.typeConfig.vpn.ipSec.psk = kTestPsk;
      assertTrue(networkConfig.vpnIsConfigured_());

      let props = networkConfig.getPropertiesToSet_();
      assertEquals(kTestVpnName, props.name);
      assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
      assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
      assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
      assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
      assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
      assertEquals(kTestPsk, props.typeConfig.vpn.ipSec.psk);
      assertEquals('', props.typeConfig.vpn.ipSec.localIdentity);
      assertEquals('', props.typeConfig.vpn.ipSec.remoteIdentity);

      networkConfig.set('vpnSaveCredentials_', true);
      assertTrue(networkConfig.getPropertiesToSet_()
                     .typeConfig.vpn.ipSec.saveCredentials);

      configProperties.typeConfig.vpn.ipSec.localIdentity = 'local-id';
      configProperties.typeConfig.vpn.ipSec.remoteIdentity = 'remote-id';
      props = networkConfig.getPropertiesToSet_();
      assertEquals('local-id', props.typeConfig.vpn.ipSec.localIdentity);
      assertEquals('remote-id', props.typeConfig.vpn.ipSec.remoteIdentity);
    });

    // Checks if values are read correctly for an existing service of PSK
    // authentication.
    test('Existing PSK', function() {
      const ikev2 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'someguid', kTestVpnName);
      ikev2.typeProperties.vpn.type = VpnType.kIKEv2;
      ikev2.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      ikev2.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'PSK'},
        ikeVersion: {activeValue: 2},
        localIdentity: {activeValue: 'local-id'},
        remoteIdentity: {activeValue: 'remote-id'},
        saveCredentials: {activeValue: true},
      };
      setNetworkConfig(ikev2);
      initNetworkConfig();

      return flushAsync().then(() => {
        assertEquals('IKEv2', networkConfig.get('vpnType_'));
        assertEquals('PSK', networkConfig.get('ipsecAuthType_'));

        // Populate the properties again. The values should be the same to what
        // are set above.
        const props = networkConfig.getPropertiesToSet_();
        assertEquals('someguid', props.guid);
        assertEquals(kTestVpnName, props.name);
        assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
        assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
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
        return flushAsync().then(() => {
          // The first Server CA and User certificate should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be true.
          setMandatoryFields();
          assertTrue(networkConfig.vpnIsConfigured_());

          const props = networkConfig.getPropertiesToSet_();
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
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
      ikev2.typeProperties.vpn.type = VpnType.kIKEv2;
      ikev2.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      ikev2.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'Cert'},
        clientCertType: {activeValue: 'PKCS11Id'},
        clientCertPkcs11Id: {activeValue: kUserCertId},
        ikeVersion: {activeValue: 2},
        saveCredentials: {activeValue: true},
        serverCaPems: {activeValue: [kCaPem]},
      };
      setNetworkConfig(ikev2);
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('IKEv2', networkConfig.get('vpnType_'));
          assertEquals('Cert', networkConfig.get('ipsecAuthType_'));
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          const props = networkConfig.getPropertiesToSet_();
          assertEquals('someguid', props.guid);
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
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
        return flushAsync().then(() => {
          // Server CA should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);

          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
          const eapProperties = networkConfig.get('eapProperties_');
          eapProperties.identity = kTestUsername;
          eapProperties.password = kTestPassword;
          assertTrue(networkConfig.vpnIsConfigured_());

          // Server CA is also mandatory when using EAP.
          networkConfig.set('selectedServerCaHash_', '');
          assertFalse(networkConfig.vpnIsConfigured_());
          networkConfig.set('selectedServerCaHash_', kCaHash);

          let props = networkConfig.getPropertiesToSet_();
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertEquals('EAP', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('MSCHAPv2', props.typeConfig.vpn.ipSec.eap.outer);
          assertEquals(kTestUsername, props.typeConfig.vpn.ipSec.eap.identity);
          assertEquals(kTestPassword, props.typeConfig.vpn.ipSec.eap.password);
          assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
          assertFalse(props.typeConfig.vpn.ipSec.eap.saveCredentials);

          networkConfig.set('vpnSaveCredentials_', true);
          props = networkConfig.getPropertiesToSet_();
          assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
          assertTrue(props.typeConfig.vpn.ipSec.eap.saveCredentials);
        });
      });
    });

    test('Existing EAP', function() {
      const ikev2 = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'someguid', kTestVpnName);
      ikev2.typeProperties.vpn.type = VpnType.kIKEv2;
      ikev2.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      ikev2.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'EAP'},
        eap: {
          domainSuffixMatch: {activeValue: []},
          identity: {activeValue: kTestUsername},
          outer: {activeValue: 'MSCHAPv2'},
          saveCredentials: {activeValue: true},
          subjectAltNameMatch: {activeValue: []},
          useSystemCas: {activeValue: false},
        },
        ikeVersion: {activeValue: 2},
        saveCredentials: {activeValue: true},
        serverCaPems: {activeValue: [kCaPem]},
      };
      setNetworkConfig(ikev2);
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ false);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('IKEv2', networkConfig.get('vpnType_'));
          assertEquals('EAP', networkConfig.get('ipsecAuthType_'));
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);

          const props = networkConfig.getPropertiesToSet_();
          assertEquals('someguid', props.guid);
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(VpnType.kIKEv2, props.typeConfig.vpn.type.value);
          assertEquals('EAP', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(2, props.typeConfig.vpn.ipSec.ikeVersion);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
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
      setNetworkType(NetworkType.kVPN);
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    // Sets all mandatory fields for an L2TP/IPsec service except for server CA
    // and user certificate.
    function setMandatoryFields() {
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
      assertEquals('PSK', networkConfig.ipsecAuthType_);
      assertFalse(!!networkConfig.$$('#ipsec-local-id-input'));
      assertFalse(!!networkConfig.$$('#ipsec-remote-id-input'));
      assertTrue(!!networkConfig.$$('#ipsec-auth-type'));
      assertTrue(!!networkConfig.$$('#l2tp-username-input'));
      assertTrue(!!networkConfig.$$('#ipsec-psk-input'));
      assertFalse(!!networkConfig.$$('#vpnServerCa'));
      assertFalse(!!networkConfig.$$('#vpnUserCert'));

      // Switch the authentication type to Cert. The PSK input should be hidden
      // and the dropdowns for server CA and user certificate should appear.
      networkConfig.set('ipsecAuthType_', 'Cert');
      flush();
      assertFalse(!!networkConfig.$$('#ipsec-psk-input'));
      assertTrue(!!networkConfig.$$('#ipsec-auth-type'));
      assertTrue(!!networkConfig.$$('#l2tp-username-input'));
      assertTrue(!!networkConfig.$$('#vpnServerCa'));
      assertTrue(!!networkConfig.$$('#vpnUserCert'));

      // Switch VPN type to IKEv2 and auth type to EAP, and then back to
      // L2TP/IPsec. The auth type should be reset to PSK since EAP is not a
      // valid value.
      networkConfig.set('vpnType_', 'IKEv2');
      networkConfig.set('ipsecAuthType_', 'EAP');
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      assertEquals('PSK', networkConfig.ipsecAuthType_);
    });

    test('No Certs', function() {
      initNetworkConfig();
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          // Check that with no certificates, 'do-not-check' and 'no-certs' are
          // selected.
          assertEquals('no-certs', networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA and user cert.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
        });
      });
    });

    test('No Server CA Certs', function() {
      initNetworkConfigWithCerts(
          /* hasServerCa= */ false, /* hasUserCert= */ true);
      networkConfig.set('vpnType_', 'L2TP_IPsec');
      networkConfig.set('ipsecAuthType_', 'Cert');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          assertEquals('no-certs', networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty server CA.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
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
        return flushAsync().then(() => {
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be false
          // due to empty client cert.
          setMandatoryFields();
          assertFalse(networkConfig.vpnIsConfigured_());
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
      assertFalse(networkConfig.vpnIsConfigured_());
      configProperties.typeConfig.vpn.ipSec.psk = kTestPsk;
      assertTrue(networkConfig.vpnIsConfigured_());

      let props = networkConfig.getPropertiesToSet_();
      assertEquals(kTestVpnName, props.name);
      assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
      assertEquals(VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
      assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
      assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
      assertFalse(props.typeConfig.vpn.ipSec.saveCredentials);
      assertEquals(kTestPsk, props.typeConfig.vpn.ipSec.psk);
      assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
      assertEquals(kTestPassword, props.typeConfig.vpn.l2tp.password);

      networkConfig.set('vpnSaveCredentials_', true);
      props = networkConfig.getPropertiesToSet_();
      assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
      assertTrue(props.typeConfig.vpn.l2tp.saveCredentials);
    });

    // Checks if values are read correctly for an existing service of PSK
    // authentication.
    test('Existing PSK', function() {
      const l2tp = OncMojo.getDefaultManagedProperties(
          NetworkType.kVPN, 'someguid', kTestVpnName);
      l2tp.typeProperties.vpn.type = VpnType.kL2TPIPsec;
      l2tp.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      l2tp.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'PSK'},
        ikeVersion: {activeValue: 1},
        saveCredentials: {activeValue: true},
      };
      l2tp.typeProperties.vpn.l2tp = {
        username: {activeValue: kTestUsername},
        saveCredentials: {activeValue: true},
      };
      setNetworkConfig(l2tp);
      initNetworkConfig();

      return flushAsync().then(() => {
        assertEquals('L2TP_IPsec', networkConfig.get('vpnType_'));
        assertEquals('PSK', networkConfig.get('ipsecAuthType_'));

        // Populate the properties again. The values should be the same to what
        // are set above.
        const props = networkConfig.getPropertiesToSet_();
        assertEquals('someguid', props.guid);
        assertEquals(kTestVpnName, props.name);
        assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
        assertEquals(VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
        assertEquals('PSK', props.typeConfig.vpn.ipSec.authenticationType);
        assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.eap);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.localIdentity);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.remoteIdentity);
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
        return flushAsync().then(() => {
          // The first Server CA and User certificate should be selected.
          assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
          assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

          // Set all other mandatory fields. vpnIsConfigured_() should be true.
          setMandatoryFields();
          assertTrue(networkConfig.vpnIsConfigured_());

          const props = networkConfig.getPropertiesToSet_();
          assertEquals(kTestVpnName, props.name);
          assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
          assertEquals(VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
          assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
          assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
          assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
          assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
          assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
          assertEquals(
              kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
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
      l2tp.typeProperties.vpn.type = VpnType.kL2TPIPsec;
      l2tp.typeProperties.vpn.host = {activeValue: kTestVpnHost};
      l2tp.typeProperties.vpn.ipSec = {
        authenticationType: {activeValue: 'Cert'},
        clientCertType: {activeValue: 'PKCS11Id'},
        clientCertPkcs11Id: {activeValue: kUserCertId},
        ikeVersion: {activeValue: 1},
        saveCredentials: {activeValue: true},
        serverCaPems: {activeValue: [kCaPem]},
      };
      l2tp.typeProperties.vpn.l2tp = {
        username: {activeValue: kTestUsername},
        saveCredentials: {activeValue: true},
      };
      setNetworkConfig(l2tp);
      initNetworkConfigWithCerts(
          /* hasServerCa= */ true, /* hasUserCert= */ true);
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        assertEquals('L2TP_IPsec', networkConfig.get('vpnType_'));
        assertEquals('Cert', networkConfig.get('ipsecAuthType_'));
        assertEquals(kCaHash, networkConfig.selectedServerCaHash_);
        assertEquals(kUserHash1, networkConfig.selectedUserCertHash_);

        // Populate the properties again. The values should be the same to what
        // are set above.
        const props = networkConfig.getPropertiesToSet_();
        assertEquals('someguid', props.guid);
        assertEquals(kTestVpnName, props.name);
        assertEquals(kTestVpnHost, props.typeConfig.vpn.host);
        assertEquals(VpnType.kL2TPIPsec, props.typeConfig.vpn.type.value);
        assertEquals('Cert', props.typeConfig.vpn.ipSec.authenticationType);
        assertEquals(1, props.typeConfig.vpn.ipSec.ikeVersion);
        assertEquals(1, props.typeConfig.vpn.ipSec.serverCaPems.length);
        assertEquals(kCaPem, props.typeConfig.vpn.ipSec.serverCaPems[0]);
        assertEquals('PKCS11Id', props.typeConfig.vpn.ipSec.clientCertType);
        assertEquals(
            kUserCertId, props.typeConfig.vpn.ipSec.clientCertPkcs11Id);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.eap);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.localIdentity);
        assertEquals(undefined, props.typeConfig.vpn.ipSec.remoteIdentity);
        assertEquals(kTestUsername, props.typeConfig.vpn.l2tp.username);
        assertTrue(props.typeConfig.vpn.ipSec.saveCredentials);
        assertTrue(props.typeConfig.vpn.l2tp.saveCredentials);
      });
    });
  });
});
