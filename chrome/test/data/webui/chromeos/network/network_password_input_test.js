// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_password_input.js';

import {OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrComponentsNetworkPasswordInputTest', function() {
  /** @type {!NetworkPasswordInput|undefined} */
  let networkPassword;

  setup(function() {
    networkPassword = document.createElement('network-password-input');
    document.body.appendChild(networkPassword);
    flush();
  });

  test('Show password button', function() {
    const passwordInput = networkPassword.$$('#input');
    assertTrue(!!passwordInput);
    assertFalse(networkPassword.showPassword);
    assertEquals('password', passwordInput.type);

    const showPassword = networkPassword.$$('#icon');
    showPassword.click();

    assertTrue(networkPassword.showPassword);
    assertEquals('text', passwordInput.type);
  });

  test('Show password button is hidden', function() {
    networkPassword.disabled = true;
    flush();
    assertFalse(!!networkPassword.$$('#icon'));

    networkPassword.disabled = false;
    flush();
    assertTrue(!!networkPassword.$$('#icon'));

    networkPassword.managedProperties = {
      source: OncSource.kNone,
    };
    flush();
    assertTrue(!!networkPassword.$$('#icon'));

    networkPassword.managedProperties = {
      source: OncSource.kDevice,
    };
    flush();
    // TODO(b/328633844): Update this test to check the visibility of the "show
    // password" button when the network type is Wi-Fi.
    assertFalse(!!networkPassword.$$('#icon'));
  });

  test('Aria label', function() {
    networkPassword.ariaLabel = 'test_aria_label';
    const passwordInput = networkPassword.$$('#input');
    assertTrue(!!passwordInput);

    assertEquals('test_aria_label', passwordInput.ariaLabel);
  });
});
