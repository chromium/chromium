// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_ip_config.js';

import type {CrToggleElement} from '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import type {ManagedProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {IPConfigType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import type {NetworkIpConfigElement} from 'chrome://resources/ash/common/network/network_ip_config.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ConnectionStateType, NetworkType, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkIpConfigTest', () => {
  let ipConfig: NetworkIpConfigElement;

  function getAutoConfigIpToggle(): CrToggleElement {
    const toggle = ipConfig.shadowRoot!.querySelector<CrToggleElement>(
        '#autoConfigIpToggle');
    assertTrue(!!toggle);
    return toggle;
  }

  function createTestManagedPropertiesWithOverridenValues(
      type: NetworkType, guid: string, name: string,
      overrides: Partial<ManagedProperties>): ManagedProperties {
    return {
      ...OncMojo.getDefaultManagedProperties(type, guid, name),
      ...overrides,
    };
  }

  setup(() => {
    ipConfig = document.createElement('network-ip-config');
    document.body.appendChild(ipConfig);
    flush();
  });

  test('Enabled', () => {
    const autoConfigIpToggle = getAutoConfigIpToggle();

    // WiFi non-policy networks should enable autoConfigIpToggle.
    ipConfig.managedProperties =
        OncMojo.getDefaultManagedProperties(NetworkType.kWiFi, '', '');
    flush();
    assertFalse(autoConfigIpToggle.disabled);
  });

  test('Auto-config toggle policy enforcement', () => {
    const autoConfigIpToggle = getAutoConfigIpToggle();

    // ipAddressConfigType is not set; auto-config is toggleable.
    ipConfig.managedProperties = createTestManagedPropertiesWithOverridenValues(
        NetworkType.kWiFi, '', '', {
          ipAddressConfigType: {
            activeValue: '',
            policySource: PolicySource.kNone,
            policyValue: null,
          },
        });
    flush();
    assertFalse(autoConfigIpToggle.disabled);

    // ipAddressConfigType policy is not enforced (kNone).
    ipConfig.managedProperties = createTestManagedPropertiesWithOverridenValues(
        NetworkType.kWiFi, '', '', {
          ipAddressConfigType: {
            activeValue: 'Static',
            policySource: PolicySource.kNone,
            policyValue: null,
          },
        });
    flush();
    assertFalse(autoConfigIpToggle.disabled);

    // ipAddressConfigType policy is enforced.
    ipConfig.managedProperties = createTestManagedPropertiesWithOverridenValues(
        NetworkType.kWiFi, '', '', {
          ipAddressConfigType: {
            activeValue: 'Static',
            policySource: PolicySource.kUserPolicyEnforced,
            policyValue: null,
          },
        });
    flush();
    assertTrue(autoConfigIpToggle.disabled);
  });

  test('Show ip config textbox when automatic config is toggled off', () => {
    ipConfig.managedProperties = createTestManagedPropertiesWithOverridenValues(
        NetworkType.kEthernet, '', '', {
          ipAddressConfigType: {
            activeValue: '',
            policySource: PolicySource.kNone,
            policyValue: null,
          },
          nameServersConfigType: {
            activeValue: '',
            policySource: PolicySource.kNone,
            policyValue: null,
          },
        });
    flush();

    const autoConfigIpToggle = getAutoConfigIpToggle();
    assertFalse(autoConfigIpToggle.disabled);

    autoConfigIpToggle.click();
    flush();

    const propertyList =
        ipConfig.shadowRoot!.querySelector('network-property-list-mojo');
    assertTrue(!!propertyList);
    assertFalse(propertyList.disabled);
  });

  test('Disabled UI state', () => {
    // WiFi non-policy networks should enable autoConfigIpToggle.
    ipConfig.managedProperties = createTestManagedPropertiesWithOverridenValues(
        NetworkType.kWiFi, '', '', {
          ipAddressConfigType: {
            activeValue: 'Static',
            policySource: PolicySource.kNone,
            policyValue: null,
          },
          nameServersConfigType: {
            activeValue: '',
            policySource: PolicySource.kNone,
            policyValue: null,
          },
          staticIpConfig: {
            ipAddress: {
              activeValue: '127.0.0.1',
              policySource: PolicySource.kNone,
              policyValue: null,
            },
            gateway: null,
            nameServers: null,
            routingPrefix: null,
            type: IPConfigType.kIPv4,
            webProxyAutoDiscoveryUrl: null,
          },
        });
    flush();

    const autoConfigIpToggle = getAutoConfigIpToggle();
    const propertyList =
        ipConfig.shadowRoot!.querySelector('network-property-list-mojo');

    assertFalse(autoConfigIpToggle.disabled);
    assertFalse(propertyList!.disabled);

    ipConfig.disabled = true;

    assertTrue(autoConfigIpToggle.disabled);
    assertTrue(propertyList!.disabled);
  });

  test('Do not show toggle if network is cellular', () => {
    const getAutoConfig = () =>
        ipConfig.shadowRoot!.querySelector('#autoConfig');

    const properties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular', '');
    ipConfig.managedProperties = properties;
    flush();

    assertFalse(!!getAutoConfig());

    const wifi =
        OncMojo.getDefaultManagedProperties(NetworkType.kWiFi, 'someguid', '');
    ipConfig.managedProperties = wifi;
    flush();

    assertTrue(!!getAutoConfig());
  });

  test(
      'Do not apply observed changes for static config type when connected',
      () => {
        const ipAddress = '127.0.0.1';
        ipConfig.managedProperties =
            createTestManagedPropertiesWithOverridenValues(
                NetworkType.kWiFi, '', '', {
                  connectionState: ConnectionStateType.kConnected,
                  ipAddressConfigType: {
                    activeValue: 'Static',
                    policySource: PolicySource.kNone,
                    policyValue: null,
                  },
                  nameServersConfigType: {
                    activeValue: '',
                    policySource: PolicySource.kNone,
                    policyValue: null,
                  },
                  staticIpConfig: {
                    ipAddress: {
                      activeValue: ipAddress,
                      policySource: PolicySource.kNone,
                      policyValue: null,
                    },
                    gateway: null,
                    nameServers: null,
                    routingPrefix: null,
                    type: IPConfigType.kIPv4,
                    webProxyAutoDiscoveryUrl: null,
                  },
                });
        flush();

        const getIpAddress = () =>
            ipConfig.shadowRoot!.querySelector('network-property-list-mojo')!
                .shadowRoot!.querySelector('cr-input')!.value;
        assertEquals(ipAddress, getIpAddress());

        ipConfig.managedProperties =
            createTestManagedPropertiesWithOverridenValues(
                NetworkType.kWiFi, '', '', {
                  connectionState: ConnectionStateType.kConnected,
                  ipAddressConfigType: {
                    activeValue: 'Static',
                    policySource: PolicySource.kNone,
                    policyValue: null,
                  },
                  nameServersConfigType: {
                    activeValue: '',
                    policySource: PolicySource.kNone,
                    policyValue: null,
                  },
                  staticIpConfig: {
                    ipAddress: {
                      activeValue: '127.0.0.2',
                      policySource: PolicySource.kNone,
                      policyValue: null,
                    },
                    gateway: null,
                    nameServers: null,
                    routingPrefix: null,
                    type: IPConfigType.kIPv4,
                    webProxyAutoDiscoveryUrl: null,
                  },
                });
        flush();

        // Observed changes should not be applied if the config type is static.
        assertEquals(ipAddress, getIpAddress());
      });
});
