// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {SubjectAltName_Type} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';

suite('OncMojoTest', () => {
  test('Serialize Domain Suffix Match', () => {
    assertEquals('', OncMojo.serializeDomainSuffixMatch([]));
    assertEquals(
        'example.com', OncMojo.serializeDomainSuffixMatch(['example.com']));
    assertEquals(
        'example1.com;example2.com;example3.com',
        OncMojo.serializeDomainSuffixMatch(
            ['example1.com', 'example2.com', 'example3.com']));
  });

  test('Deserialize Domain Suffix Match', () => {
    assertDeepEquals([], OncMojo.deserializeDomainSuffixMatch(''));
    assertDeepEquals([], OncMojo.deserializeDomainSuffixMatch('  '));
    assertDeepEquals(
        ['example'], OncMojo.deserializeDomainSuffixMatch('example'));
    assertDeepEquals(
        ['example'], OncMojo.deserializeDomainSuffixMatch('example;'));
    assertDeepEquals(
        ['example1', 'example2'],
        OncMojo.deserializeDomainSuffixMatch('example1;example2'));
    // '#' is a non-RFC compliant DNS character.
    assertEquals(null, OncMojo.deserializeDomainSuffixMatch('example#'));
  });

  test('Serialize Subject Alternative Name Match', () => {
    assertEquals('', OncMojo.serializeSubjectAltNameMatch([]));
    assertEquals(
        'EMAIL:test@example.com;URI:http://test.com',
        OncMojo.serializeSubjectAltNameMatch([
          {type: SubjectAltName_Type.kEmail, value: 'test@example.com'},
          {type: SubjectAltName_Type.kUri, value: 'http://test.com'},
        ]));
  });

  test('Deserialize Subject Alternative Name Match', () => {
    assertDeepEquals([], OncMojo.deserializeSubjectAltNameMatch(''));
    assertDeepEquals([], OncMojo.deserializeSubjectAltNameMatch('  '));
    assertDeepEquals(
        [
          {type: SubjectAltName_Type.kEmail, value: 'test@example.com'},
          {type: SubjectAltName_Type.kUri, value: 'http://test.com'},
        ],
        OncMojo.deserializeSubjectAltNameMatch(
            'EMAIL:test@example.com;uri:http://test.com'));
    // Malformed SAN entry.
    assertEquals(
        null, OncMojo.deserializeSubjectAltNameMatch('EMAILtest@example.com'));
    // Incorrect SAN type.
    assertEquals(
        null, OncMojo.deserializeSubjectAltNameMatch('E:test@example.com'));
    // Non-RFC compliant character.
    assertEquals(
        null,
        OncMojo.deserializeSubjectAltNameMatch('EMAIL:test@exa\'mple.com'));
  });

  test('Baseline config properties preserve static IP configuration', () => {
    const staticConfigType = {
      activeValue: 'Static',
      policySource: PolicySource.kNone,
    };
    const managedProperties =
        OncMojo.getDefaultManagedProperties(NetworkType.kWiFi, '', '');

    let result = OncMojo.getBaselineConfigProperties(managedProperties);
    assertEquals(result.ipAddressConfigType, 'DHCP');
    assertEquals(result.nameServersConfigType, 'DHCP');
    assertFalse(!!result.staticIpConfig);

    managedProperties.ipAddressConfigType = staticConfigType;
    result = OncMojo.getBaselineConfigProperties(managedProperties);
    assertEquals(result.ipAddressConfigType, 'Static');
    assertEquals(result.nameServersConfigType, 'DHCP');
    assertFalse(!!result.staticIpConfig);

    managedProperties.nameServersConfigType = staticConfigType;
    result = OncMojo.getBaselineConfigProperties(managedProperties);
    assertEquals(result.ipAddressConfigType, 'Static');
    assertEquals(result.nameServersConfigType, 'Static');
    assertFalse(!!result.staticIpConfig);

    const staticIpConfig = {
      nameServers: ['1.2.3.4'],
    };
    managedProperties.staticIpConfig = staticIpConfig;
    result = OncMojo.getBaselineConfigProperties(managedProperties);
    assertEquals(result.ipAddressConfigType, 'Static');
    assertEquals(result.nameServersConfigType, 'Static');
    assertDeepEquals(result.staticIpConfig, staticIpConfig);
  });
});
