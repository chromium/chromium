// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_config_select.js';

import {SecurityType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkConfigSelectTest', function() {
  /** @type {!NetworkConfigSelect|undefined} */
  let configSelect;

  setup(function() {
    configSelect = document.createElement('network-config-select');
    configSelect.oncPrefix = 'Cellular.ActivationState';
    document.body.appendChild(configSelect);
    flush();
  });

  test('Item enabled state', function() {
    assertTrue(!!configSelect);

    // If the select does not contain a list of certs, the items are always
    // enabled.
    configSelect.certList = false;

    const selectEl = configSelect.$$('select');
    assertTrue(!!selectEl);

    // Add a non-cert item.
    configSelect.items = ['Activated'];
    flush();

    const optionEl = configSelect.$$('option');
    assertTrue(!!optionEl);

    // Any non-cert item is enabled.
    let optionEnabled = !optionEl.disabled;
    assertTrue(optionEnabled);

    // Set the select to contain certs.
    configSelect.certList = true;

    // NetworkCertificate
    configSelect.items = [
      {deviceWide: true, hash: 'hash', issuedBy: 'me'},
    ];
    flush();

    optionEnabled = !optionEl.disabled;
    assertTrue(optionEnabled);

    // Unset the hash; the item is disabled without a hash.
    configSelect.items = [
      {deviceWide: true, hash: null, issuedBy: 'me'},
    ];
    flush();
    optionEnabled = !optionEl.disabled;
    assertFalse(optionEnabled);

    // Unset the hash; the item is disabled without a hash.
    configSelect.items = [
      {deviceWide: true, hash: null, issuedBy: 'me'},
    ];
    flush();
    optionEnabled = !optionEl.disabled;
    assertFalse(optionEnabled);

    // Only allow device certs in the list. The cert in the list is device-wide
    // so matches the criteria to be enabled.
    configSelect.deviceCertsOnly = true;
    configSelect.items = [
      {deviceWide: true, hash: 'hash', issuedBy: 'me'},
    ];
    flush();
    optionEnabled = !optionEl.disabled;
    assertTrue(optionEnabled);

    // Change the cert to not be device-wide.
    configSelect.items = [
      {deviceWide: false, hash: 'hash', issuedBy: 'me'},
    ];
    flush();
    optionEnabled = !optionEl.disabled;
    assertFalse(optionEnabled);
  });

  test('Validation for Pre-filled value', function() {
    assertTrue(!!configSelect);

    configSelect.key = 'security';
    configSelect.oncPrefix = 'WiFi.Security';
    configSelect.items = [SecurityType.kNone, SecurityType.kWpaEap];

    const testCases = [
      {prefilledValue: null, shouldBeValid: false},
      {prefilledValue: SecurityType.kWepPsk, shouldBeValid: false},
      {prefilledValue: SecurityType.kNone, shouldBeValid: true},
      {prefilledValue: SecurityType.kWpaEap, shouldBeValid: true},
    ];
    for (const {prefilledValue, shouldBeValid} of testCases) {
      configSelect.prefilledValue = prefilledValue;
      if (shouldBeValid) {
        assertEquals(configSelect.value, configSelect.prefilledValue);
        assertTrue(configSelect.disabled);
      } else {
        assertNotEquals(configSelect.value, configSelect.prefilledValue);
        assertFalse(configSelect.disabled);
      }
    }
  });
});
