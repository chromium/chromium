// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_nameservers.js';

import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import type {CrRadioButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import type {CrRadioGroupElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import type {NetworkNameserversElement} from 'chrome://resources/ash/common/network/network_nameservers.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {IPConfigType, NetworkType, PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkNameserversTest', () => {
  let nameservers: NetworkNameserversElement|undefined;

  setup(() => {
    nameservers = document.createElement('network-nameservers');
    document.body.appendChild(nameservers);
    flush();
  });

  test('Select nameservers', async () => {
    assertTrue(!!nameservers);
    const nameserverTypeRadioGroup =
        nameservers.shadowRoot!.querySelector<CrRadioGroupElement>(
            '#nameserverType');
    assertTrue(!!nameserverTypeRadioGroup);

    // Default nameserver type is 'automatic'.
    const automaticButton =
        nameservers.shadowRoot!.querySelector<CrRadioButtonElement>(
            'cr-radio-button[name=automatic]');
    assertTrue(!!automaticButton);
    const googleButton =
        nameservers.shadowRoot!.querySelector<CrRadioButtonElement>(
            'cr-radio-button[name=google]');
    assertTrue(!!googleButton);
    const customButton =
        nameservers.shadowRoot!.querySelector<CrRadioButtonElement>(
            'cr-radio-button[name=custom]');
    assertTrue(!!customButton);

    assertEquals('automatic', nameserverTypeRadioGroup.selected);
    assertTrue(automaticButton.checked);
    assertFalse(googleButton.checked);
    assertFalse(customButton.checked);

    nameserverTypeRadioGroup.selected = 'google';
    assertFalse(automaticButton.checked);
    assertTrue(googleButton.checked);
    assertFalse(customButton.checked);

    nameserverTypeRadioGroup.selected = 'custom';
    assertFalse(automaticButton.checked);
    assertFalse(googleButton.checked);
    assertTrue(customButton.checked);
  });

  test('Disabled UI state', () => {
    assertTrue(!!nameservers);
    const nameserverTypeRadioGroup =
        nameservers.shadowRoot!.querySelector<CrRadioGroupElement>(
            '#nameserverType');
    assertTrue(!!nameserverTypeRadioGroup);
    assertFalse(nameserverTypeRadioGroup.disabled);

    nameservers.disabled = true;

    assertTrue(nameserverTypeRadioGroup.disabled);
  });

  test(
      'Do not apply observed changes for static config type when connected',
      () => {
        assertTrue(!!nameservers);
        const nameserverTypeRadioGroup =
            nameservers.shadowRoot!.querySelector<CrRadioGroupElement>(
                '#nameserverType');
        assertTrue(!!nameserverTypeRadioGroup);

        const ipAddress = '8.8.8.2';
        nameserverTypeRadioGroup.selected = 'custom';

        const managedProperties = OncMojo.getDefaultManagedProperties(
            NetworkType.kEthernet, 'f19a0128-0b37-490a-bfc9-d04031f27d2a',
            'name');
        managedProperties.staticIpConfig = {
          gateway: undefined,
          ipAddress: undefined,
          nameServers: {
            activeValue: ['8.8.8.2', '8.8.8.8', '0.0.0.0', '0.0.0.0'],
            policySource: PolicySource.kNone,
            policyValue: undefined,
          },
          routingPrefix: undefined,
          type: IPConfigType.kIPv4,
          webProxyAutoDiscoveryUrl: undefined,
        };
        managedProperties.ipAddressConfigType.activeValue = 'DHCP';
        managedProperties.nameServersConfigType.activeValue = 'Static';
        nameservers.managedProperties = managedProperties;
        flush();

        const customNameServerInput =
            nameservers.shadowRoot!.querySelector<CrInputElement>(
                'cr-input[id=nameserver0]');
        assertTrue(!!customNameServerInput);
        assertEquals(ipAddress, customNameServerInput.value);

        managedProperties.staticIpConfig.nameServers!.activeValue =
            ['0.0.0.2', '8.8.8.8', '0.0.0.0', '0.0.0.0'];
        flush();

        assertEquals(ipAddress, customNameServerInput.value);
      });
});
