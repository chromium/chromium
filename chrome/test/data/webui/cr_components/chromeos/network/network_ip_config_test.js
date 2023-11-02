// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_ip_config.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ConnectionStateType, NetworkType, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NetworkIpConfigTest', function() {
  /** @type {!NetworkIpConfig|undefined} */
  let ipConfig;

  setup(function() {
    ipConfig = document.createElement('network-ip-config');
    document.body.appendChild(ipConfig);
    flush();
  });

  test('Enabled', function() {
    assertTrue(!!ipConfig.$$('#autoConfigIpToggle'));
    // WiFi non-policy networks should enable autoConfigIpToggle.
    ipConfig.managedProperties = {
      ipAddressConfigType: {
        activeValue: 'Static',
        policySource: PolicySource.kNone,
      },
      type: NetworkType.kWiFi,
    };
    flush();
    assertFalse(ipConfig.$$('#autoConfigIpToggle').disabled);
  });

  test('Auto-config toggle policy enforcement', function() {
    assertTrue(!!ipConfig.$$('#autoConfigIpToggle'));

    // ipAddressConfigType is not set; auto-config is toggleable.
    ipConfig.managedProperties = {
      ipAddressConfigType: null,
    };
    flush();
    assertFalse(ipConfig.$$('#autoConfigIpToggle').disabled);

    // ipAddressConfigType policy is not enforced (kNone).
    ipConfig.managedProperties = {
      ipAddressConfigType: {
        activeValue: 'Static',
        policySource: PolicySource.kNone,
      },
    };
    flush();
    assertFalse(ipConfig.$$('#autoConfigIpToggle').disabled);

    // ipAddressConfigType policy is enforced.
    ipConfig.managedProperties = {
      ipAddressConfigType: {
        activeValue: 'Static',
        policySource: PolicySource.kUserPolicyEnforced,
      },
    };
    flush();
    assertTrue(ipConfig.$$('#autoConfigIpToggle').disabled);
  });

  test('Disabled UI state', function() {
    // WiFi non-policy networks should enable autoConfigIpToggle.
    ipConfig.managedProperties = {
      ipAddressConfigType: {
        activeValue: 'Static',
        policySource: PolicySource.kNone,
      },
      staticIpConfig: {
        ipAddress: {
          activeValue: '127.0.0.1',
        },
      },
      connectionState: ConnectionStateType.kNotConnected,
      type: NetworkType.kWiFi,
    };
    flush();

    const autoConfigIpToggle = ipConfig.$$('#autoConfigIpToggle');
    const propertyList = ipConfig.$$('network-property-list-mojo');

    assertFalse(autoConfigIpToggle.disabled);
    assertFalse(propertyList.disabled);

    ipConfig.disabled = true;

    assertTrue(autoConfigIpToggle.disabled);
    assertTrue(propertyList.disabled);
  });

  test('Do not show toggle if network is cellular', function() {
    const getAutoConfig = () => ipConfig.$$('#autoConfig');

    const properties =
        OncMojo.getDefaultManagedProperties(NetworkType.kCellular, 'cellular');
    ipConfig.managedProperties = properties;
    flush();

    assertFalse(!!getAutoConfig());

    const wifi =
        OncMojo.getDefaultManagedProperties(NetworkType.kWiFi, 'someguid', '');
    ipConfig.managedProperties = wifi;
    flush();

    assertTrue(!!getAutoConfig());
  });
});
