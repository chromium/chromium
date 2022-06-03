// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_password_input.m.js';
//
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('CrComponentsNetworkPasswordInputTest', function() {
  /** @type {!NetworkPasswordInput|undefined} */
  let networkPassword;

  setup(function() {
    networkPassword = document.createElement('network-password-input');
    document.body.appendChild(networkPassword);
    Polymer.dom.flush();
  });

  test('Show password button', function() {
    let passwordInput = networkPassword.$$('#input');
    assertTrue(!!passwordInput);
    assertFalse(networkPassword.showPassword);
    assertEquals("password", passwordInput.type);

    let showPassword = networkPassword.$$('#icon');
    showPassword.click();

    assertTrue(networkPassword.showPassword);
    assertEquals("text", passwordInput.type);
  });
});
