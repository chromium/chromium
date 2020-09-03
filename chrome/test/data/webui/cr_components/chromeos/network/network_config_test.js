// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_config.m.js';

// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
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
          // Check that with no certificates, 'do-not-check' amd 'no-certs'
          // are selected.
          assertEquals('do-not-check', networkConfig.selectedServerCaHash_);
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
          [{hash: kCaHash, hardwareBacked: true, deviceWide: true}],
          [{hash: kUserHash1, hardwareBacked: true, deviceWide: false}]);
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
          [{hash: kCaHash, hardwareBacked: true, deviceWide: true}], [
            {hash: kUserHash1, hardwareBacked: true, deviceWide: false},
            {hash: kUserHash2, hardwareBacked: true, deviceWide: true}
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
