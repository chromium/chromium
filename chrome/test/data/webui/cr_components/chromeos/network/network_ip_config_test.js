// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_ip_config.m.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkIpConfigTest', function() {
  /** @type {!NetworkIpConfig|undefined} */
  let ipConfig;

  setup(function() {
    ipConfig = document.createElement('network-ip-config');
    document.body.appendChild(ipConfig);
    Polymer.dom.flush();
  });

  test('Enabled', function() {
    const mojom = chromeos.networkConfig.mojom;
    assertTrue(!!ipConfig.$.autoConfigIpToggle);

    // WiFi non-policy networks should enable autoConfigIpToggle.
    ipConfig.managedProperties = {
      ipAddressConfigType: {
        activeValue: 'Static',
        policySource: mojom.PolicySource.kNone,
      },
      type: mojom.NetworkType.kWiFi,
    };
    Polymer.dom.flush();
    assertFalse(ipConfig.$.autoConfigIpToggle.disabled);

    // Cellular network should disable autoConfigIpToggle.
    ipConfig.managedProperties = {
      ipAddressConfigType: {
        activeValue: 'Static',
        policySource: mojom.PolicySource.kNone,
      },
      type: mojom.NetworkType.kCellular,
    };
    Polymer.dom.flush();
    assertTrue(ipConfig.$.autoConfigIpToggle.disabled);
  });

  test('Auto-config toggle policy enforcement', function() {
    const mojom = chromeos.networkConfig.mojom;

    assertTrue(!!ipConfig.$.autoConfigIpToggle);

    // ipAddressConfigType is not set; auto-config is toggleable.
    ipConfig.managedProperties = {
      ipAddressConfigType: null,
    };
    Polymer.dom.flush();
    assertFalse(ipConfig.$.autoConfigIpToggle.disabled);

    // ipAddressConfigType policy is not enforced (kNone).
    ipConfig.managedProperties = {
      ipAddressConfigType: {
        activeValue: 'Static',
        policySource: mojom.PolicySource.kNone,
      },
    };
    Polymer.dom.flush();
    assertFalse(ipConfig.$.autoConfigIpToggle.disabled);

    // ipAddressConfigType policy is enforced.
    ipConfig.managedProperties = {
      ipAddressConfigType: {
        activeValue: 'Static',
        policySource: mojom.PolicySource.kUserPolicyEnforced,
      },
    };
    Polymer.dom.flush();
    assertTrue(ipConfig.$.autoConfigIpToggle.disabled);
  });

  test('Disabled UI state', function() {
    const mojom = chromeos.networkConfig.mojom;
    // WiFi non-policy networks should enable autoConfigIpToggle.
    ipConfig.managedProperties = {
      ipAddressConfigType: {
        activeValue: 'Static',
        policySource: mojom.PolicySource.kNone,
      },
      staticIpConfig: {
        ipAddress: {
          activeValue: '127.0.0.1',
        },
      },
      connectionState: mojom.ConnectionStateType.kNotConnected,
      type: mojom.NetworkType.kWiFi,
    };
    Polymer.dom.flush();

    const autoConfigIpToggle = ipConfig.$.autoConfigIpToggle;
    const propertyList = ipConfig.$$('network-property-list-mojo');

    assertFalse(autoConfigIpToggle.disabled);
    assertFalse(propertyList.disabled);

    ipConfig.disabled = true;

    assertTrue(autoConfigIpToggle.disabled);
    assertTrue(propertyList.disabled);
  });
});
