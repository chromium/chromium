// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_property_list_mojo.js';

import {FAKE_CREDENTIAL} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkPropertyListMojoTest', function() {
  /** @type {!NetworkPropertyListMojo|undefined} */
  let propertyList;

  setup(function() {
    propertyList = document.createElement('network-property-list-mojo');
    const ipv4 = {
      ipAddress: '100.0.0.1',
      type: 'IPv4',
    };
    const tether = {
      signalStrength: 0,
    };

    propertyList.propertyDict = {ipv4, typeProperties: {tether}};
    propertyList.fields = [
      'ipv4.ipAddress',
      'ipv4.routingPrefix',
      'ipv4.gateway',
      'ipv6.ipAddress',
      `tether.signalStrength`,
    ];

    document.body.appendChild(propertyList);
    flush();
  });

  function verifyEditableFieldTypes() {
    // ipv4.ipAddress is not set as editable (via |editFieldTypes|), so the
    // edit input does not exist.
    assertEquals(null, propertyList.$$('cr-input'));

    // Set ipv4.ipAddress field as editable.
    propertyList.editFieldTypes = {
      'ipv4.ipAddress': 'String',
    };
    flush();

    // The input to edit the property now exists.
    assertNotEquals(null, propertyList.$$('cr-input'));
  }

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  async function simulatePropertyChange(latestIp) {
    propertyList.propertyDict = {
      ipv4: {
        ipAddress: latestIp,
        type: 'IPv4',
      },
    };
    await flushAsync();
  }

  function simulateSignalStrengthChange(latestSignalStrength) {
    const updatedPropertyDict = {...propertyList.propertyDict};
    updatedPropertyDict.typeProperties.tether.signalStrength =
        latestSignalStrength;
    propertyList.propertyDict = updatedPropertyDict;
  }

  test(
      'Initial input text selection without subsequent property changes',
      async () => {
        verifyEditableFieldTypes();

        // The first CrInputElement has the not been edited.
        assertEquals(
            propertyList.$$('cr-input').getAttribute('edited'), 'false');

        const crInput = propertyList.$$('cr-input');

        // Simulate user switching off automatic.
        propertyList.allFieldsReadOnly = false;
        await flushAsync();

        // The CrInputElement is focused and its text focused.
        assertEquals(
            propertyList.$$('cr-input').getAttribute('edited'), 'true');
        assertEquals('100.0.0.1', crInput.value);
        assertEquals('100.0.0.1', window.getSelection().toString());
        assertEquals(crInput, propertyList.shadowRoot.activeElement);

        // After editing the first time, subsequent focuses will not select
        // the entire contents.
        crInput.value = 'fake input';
        crInput.blur();
        crInput.focusInput();
        assertEquals('', window.getSelection().toString());
        assertEquals(crInput, propertyList.shadowRoot.activeElement);
      });

  test(
      'Initial input text selection after unpredictable property changes',
      async () => {
        verifyEditableFieldTypes();

        // The first CrInputElement has the not been edited.
        assertEquals(
            propertyList.$$('cr-input').getAttribute('edited'), 'false');

        // Simulate user switching off automatic.
        propertyList.allFieldsReadOnly = false;
        flush();

        // Focus event has not fired, and the edited attribute still remains
        // false.
        const crInput = propertyList.$$('cr-input');
        assertEquals(
            propertyList.$$('cr-input').getAttribute('edited'), 'false');
        assertEquals('100.0.0.1', propertyList.$$('cr-input').value);

        // Simulate immediate change in the properties.
        const latestIp = '100.0.0.2';
        await simulatePropertyChange(latestIp);

        // The CrInputElement is focused and its text focused with the latest
        // property information.
        assertEquals(
            propertyList.$$('cr-input').getAttribute('edited'), 'true');
        assertEquals(latestIp, window.getSelection().toString());
        assertEquals(latestIp, crInput.value);
        assertEquals(crInput, propertyList.shadowRoot.activeElement);

        // Simulate new input.
        crInput.value = 'fake input';
        crInput.blur();

        // Ensure that new changes to the properties do not cause focus and
        // selection after edits have been made.
        await simulatePropertyChange('100.0.0.3');
        assertEquals('', window.getSelection().toString());
        assertNotEquals(crInput, propertyList.shadowRoot.activeElement);
      });

  test('Disabled UI state', function() {
    // Create editable property.
    verifyEditableFieldTypes();

    const input = propertyList.$$('cr-input');
    assertFalse(input.disabled);

    propertyList.disabled = true;

    assertTrue(input.disabled);
  });

  test('Fake credential placeholder', async () => {
    await simulatePropertyChange(FAKE_CREDENTIAL);
    const propertyValues = Array.from(
        propertyList.shadowRoot.querySelectorAll('.cr-secondary-text'));
    assertTrue(propertyValues.some(
        element => element.textContent.trim() === FAKE_CREDENTIAL));
    propertyValues.forEach(element => {
      const textSecurity =
          getComputedStyle(element).getPropertyValue('-webkit-text-security');
      const textValue = element.textContent.trim();
      assertTrue(textSecurity === 'disc' || textValue !== FAKE_CREDENTIAL);
    });
  });

  test('Tether network signal strength updates', () => {
    const element = propertyList.shadowRoot.querySelector(
        '.cr-secondary-text[data-key="tether.signalStrength"]');
    assertEquals(
        propertyList.i18n('OncTether-SignalStrength_None'),
        element.textContent.trim());

    simulateSignalStrengthChange(25);
    assertEquals(
        propertyList.i18n('OncTether-SignalStrength_Low'),
        element.textContent.trim());

    simulateSignalStrengthChange(50);
    assertEquals(
        propertyList.i18n('OncTether-SignalStrength_Medium'),
        element.textContent.trim());

    simulateSignalStrengthChange(95);
    assertEquals(
        propertyList.i18n('OncTether-SignalStrength_Strong'),
        element.textContent.trim());

    simulateSignalStrengthChange(100);
    assertEquals(
        propertyList.i18n('OncTether-SignalStrength_Strong'),
        element.textContent.trim());
  });
});
