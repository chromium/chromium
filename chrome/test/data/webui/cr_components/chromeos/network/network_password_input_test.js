// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_password_input.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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
});
