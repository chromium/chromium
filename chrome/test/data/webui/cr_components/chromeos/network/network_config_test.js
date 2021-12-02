// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_config.m.js';

// #import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {eventToPromise} from '../../../test_util.js';
// clang-format on

suite('network-config', function() {
  var networkConfig;

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  const kCaHash = 'CAHASH';
  const kUserHash1 = 'USERHASH1';
  const kUserHash2 = 'USERHASH2';

  suiteSetup(function() {
    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
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
    Polymer.dom.flush();
  }

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      networkConfig.async(resolve);
    });
  }

  /**
   * Simulate an element of id |elementId| fires enter event.
   * @param {string} elementId
   */
  function simulateEnterPressedInElement(elementId) {
    let element = networkConfig.$$(`#${elementId}`);
    networkConfig.connectOnEnter = true;
    assertTrue(!!element);
    element.fire('enter', {path: [element]});
  }

  suite('New WiFi Config', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kWiFi);
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
      networkConfig.$$('#security').value =
          chromeos.networkConfig.mojom.SecurityType.kWpaPsk;
      return flushAsync().then(() => {
        assertTrue(!!networkConfig.$$('#wifi-passphrase'));
      });
    });
  });

  suite('Existing WiFi Config', function() {
    setup(function() {
      mojoApi_.resetForTest();
      const wifi1 = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kWiFi, 'someguid', '');
      wifi1.name = OncMojo.createManagedString('somename');
      wifi1.source = chromeos.networkConfig.mojom.OncSource.kDevice;
      wifi1.typeProperties.wifi.security =
          chromeos.networkConfig.mojom.SecurityType.kWepPsk;
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
        let passwordInput = networkConfig.$$('#wifi-passphrase');
        assertTrue(!!passwordInput);
        assertTrue(!!networkConfig.error);

        passwordInput.fire('keypress');
        Polymer.dom.flush();
        assertFalse(!!networkConfig.error);
      });
    });
  });

  suite('WireGuard', function() {
    setup(function() {
      mojoApi_.resetForTest();
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kVPN);
      initNetworkConfig();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Switch VPN Type', function() {
      const configProperties = networkConfig.get('configProperties_');
      networkConfig.set('vpnType_', 'OpenVPN');
      Polymer.dom.flush();
      assertFalse(!!configProperties.typeConfig.vpn.wireguard);
      assertFalse(!!networkConfig.$$('#wireguard-ip-input'));
      networkConfig.set('vpnType_', 'WireGuard');
      Polymer.dom.flush();
      assertFalse(!!configProperties.typeConfig.vpn.openvpn);
      assertTrue(!!configProperties.typeConfig.vpn.wireguard);
      assertTrue(!!networkConfig.$$('#wireguard-ip-input'));
    });

    test('Switch key config type', function() {
      networkConfig.set('vpnType_', 'WireGuard');
      Polymer.dom.flush();
      assertFalse(!!networkConfig.$$('#wireguardPrivateKeyInput'));
      networkConfig.set('wireguardKeyType_', 'UserInput');
      return flushAsync().then(() => {
        assertTrue(!!networkConfig.$$('#wireguardPrivateKeyInput'));
      });
    });

    test('Enable Connect', function() {
      networkConfig.set('vpnType_', 'WireGuard');
      Polymer.dom.flush();
      assertFalse(networkConfig.enableConnect);
      networkConfig.set('ipAddressInput_', '10.10.0.1');
      const configProperties = networkConfig.get('configProperties_');
      configProperties.name = 'test-wireguard';
      const peer = configProperties.typeConfig.vpn.wireguard.peers[0];
      peer.publicKey = 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=';
      assertFalse(networkConfig.vpnIsConfigured_());
      peer.endpoint = '192.168.66.66:32000';
      peer.allowedIps = '0.0.0.0/0';
      assertTrue(networkConfig.vpnIsConfigured_());
      peer.presharedKey = 'invalid_key';
      assertFalse(networkConfig.vpnIsConfigured_());
      peer.presharedKey = '';
      assertTrue(networkConfig.vpnIsConfigured_());
    });
  });

  suite('Existing WireGuard', function() {
    setup(function() {
      mojoApi_.resetForTest();
      const wg1 = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kVPN, 'someguid', '');
      wg1.typeProperties.vpn.type =
          chromeos.networkConfig.mojom.VpnType.kWireGuard;
      wg1.typeProperties.vpn.wireguard = {
        peers: {
          activeValue: [{
            publicKey: 'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=',
            endpoint: '192.168.66.66:32000',
            allowedIps: '0.0.0.0/0',
          }]
        }
      };
      wg1.staticIpConfig = {ipAddress: {activeValue: '10.10.0.1'}};
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
        assertEquals('10.10.0.1', networkConfig.get('ipAddressInput_'));
        assertEquals(
            'KFhwdv4+jKpSXMW6xEUVtOe4Mo8l/xOvGmshmjiHx1Y=', peer.publicKey);
        assertEquals('192.168.66.66:32000', peer.endpoint);
        assertEquals('0.0.0.0/0', peer.allowedIps);
      });
    });
  });

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

    test('New Config: Login or guest', function() {
      // Insecure networks are always shared so test a secure config.
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWepPsk);
      setLoginOrGuest();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Kiosk', function() {
      // Insecure networks are always shared so test a secure config.
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWepPsk);
      setKiosk();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Not secure', function() {
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kWiFi);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Authenticated, Secure', function() {
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWepPsk);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertFalse(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Not secure to secure', async function() {
      // set default to insecure network
      setNetworkType(chromeos.networkConfig.mojom.NetworkType.kWiFi);
      setAuthenticated();
      initNetworkConfig();
      await flushAsync();
      let share = networkConfig.$$('#share');
      assertTrue(!!share);
      assertTrue(share.disabled);
      assertTrue(share.checked);

      // change to secure network
      networkConfig.securityType_ =
          chromeos.networkConfig.mojom.SecurityType.kWepPsk;
      await flushAsync();
      assertTrue(!!share);
      assertFalse(share.disabled);
      assertFalse(share.checked);
    });

    // Existing networks hide the shared control in the config UI.
    test('Existing Hides Shared', function() {
      const wifi1 = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kWiFi, 'someguid', '');
      wifi1.source = chromeos.networkConfig.mojom.OncSource.kUser;
      wifi1.typeProperties.wifi.security =
          chromeos.networkConfig.mojom.SecurityType.kWepPsk;
      setNetworkConfig(wifi1);
      setAuthenticated();
      initNetworkConfig();
      return flushAsync().then(() => {
        assertFalse(!!networkConfig.$$('#share'));
      });
    });

    test('Ethernet', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kEthernet, 'ethernetguid',
          '');
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('None');
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('ethernetguid', networkConfig.guid);
        assertEquals(
            chromeos.networkConfig.mojom.SecurityType.kNone,
            networkConfig.securityType_);
        let outer = networkConfig.$$('#outer');
        assertFalse(!!outer);
      });
    });

    test('Ethernet EAP', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kEthernet, 'eapguid', '');
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('8021x');
      eth.typeProperties.ethernet.eap = {
        outer: OncMojo.createManagedString('PEAP')
      };
      setNetworkConfig(eth);
      initNetworkConfig();
      return flushAsync().then(() => {
        assertEquals('eapguid', networkConfig.guid);
        assertEquals(
            chromeos.networkConfig.mojom.SecurityType.kWpaEap,
            networkConfig.securityType_);
        assertEquals(
            'PEAP',
            networkConfig.managedProperties.typeProperties.ethernet.eap.outer
                .activeValue);
        assertEquals(
            'PEAP',
            networkConfig.configProperties_.typeConfig.ethernet.eap.outer);
        assertEquals('PEAP', networkConfig.eapProperties_.outer);
        let outer = networkConfig.$$('#outer');
        assertTrue(!!outer);
        assertTrue(!outer.disabled);
        assertEquals('PEAP', outer.value);
      });
    });

    test('Ethernet input fires enter event on keydown', function() {
      const eth = OncMojo.getDefaultManagedProperties(
          chromeos.networkConfig.mojom.NetworkType.kEthernet, 'eapguid', '');
      eth.typeProperties.ethernet.authentication =
          OncMojo.createManagedString('8021x');
      eth.typeProperties.ethernet.eap = {
        outer: OncMojo.createManagedString('PEAP')
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

  suite('Certificates', function() {
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

    test('WiFi EAP-TLS No Certs', function() {
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWpaEap);
      setAuthenticated();
      initNetworkConfig();
      networkConfig.shareNetwork_ = false;
      networkConfig.set('eapProperties_.outer', 'EAP-TLS');
      return mojoApi_.whenCalled('getNetworkCertificates').then(() => {
        return flushAsync().then(() => {
          let outer = networkConfig.$$('#outer');
          assertEquals('EAP-TLS', outer.value);
          // Check that with no certificates, 'default' and 'no-certs' are
          // selected.
          assertEquals('default', networkConfig.selectedServerCaHash_);
          assertEquals('no-certs', networkConfig.selectedUserCertHash_);
        });
      });
    });

    test('WiFi EAP-TLS Certs', function() {
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true
          }],
          [{
            hash: kUserHash1,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: false
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
      setNetworkType(
          chromeos.networkConfig.mojom.NetworkType.kWiFi,
          chromeos.networkConfig.mojom.SecurityType.kWpaEap);
      setAuthenticated();
      mojoApi_.setCertificatesForTest(
          [{
            hash: kCaHash,
            availableForNetworkAuth: true,
            hardwareBacked: true,
            deviceWide: true
          }],
          [
            {
              hash: kUserHash1,
              availableForNetworkAuth: true,
              hardwareBacked: true,
              deviceWide: false
            },
            {
              hash: kUserHash2,
              availableForNetworkAuth: true,
              hardwareBacked: true,
              deviceWide: true
            }
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
  });
});
