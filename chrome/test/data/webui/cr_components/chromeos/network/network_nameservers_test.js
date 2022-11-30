// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_nameservers.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

suite('NetworkNameserversTest', function() {
  /** @type {!NetworkNameservers|undefined} */
  let nameservers;

  setup(function() {
    nameservers = document.createElement('network-nameservers');
    document.body.appendChild(nameservers);
    flush();
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
