// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_config_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
});
