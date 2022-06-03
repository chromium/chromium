// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_nameservers.m.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkNameserversTest', function() {
  /** @type {!NetworkNameservers|undefined} */
  let nameservers;

  setup(function() {
    nameservers = document.createElement('network-nameservers');
    document.body.appendChild(nameservers);
    Polymer.dom.flush();
  });

  test('Select nameservers', async () => {
    assertTrue(!!nameservers.$.nameserverType);

    // Default nameserver type is 'automatic'.
    assertEquals('automatic', nameservers.$.nameserverType.selected);
    assertTrue(nameservers.$$('cr-radio-button[name=automatic]').checked);
    assertFalse(nameservers.$$('cr-radio-button[name=google]').checked);
    assertFalse(nameservers.$$('cr-radio-button[name=custom]').checked);

    nameservers.$.nameserverType.selected = 'google';
    assertFalse(nameservers.$$('cr-radio-button[name=automatic]').checked);
    assertTrue(nameservers.$$('cr-radio-button[name=google]').checked);
    assertFalse(nameservers.$$('cr-radio-button[name=custom]').checked);

    nameservers.$.nameserverType.selected = 'custom';
    assertFalse(nameservers.$$('cr-radio-button[name=automatic]').checked);
    assertFalse(nameservers.$$('cr-radio-button[name=google]').checked);
    assertTrue(nameservers.$$('cr-radio-button[name=custom]').checked);
  });

  test('Disabled UI state', function() {
    const radioGroup = nameservers.$.nameserverType;
    assertFalse(radioGroup.disabled);

    nameservers.disabled = true;

    assertTrue(radioGroup.disabled);
  });
});
