// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_password_input.js';

import type {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import type {NetworkPasswordInputElement} from 'chrome://resources/ash/common/network/network_password_input.js';
import {FAKE_CREDENTIAL} from 'chrome://resources/ash/common/network/onc_mojo.js';
import type {ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrComponentsNetworkPasswordInputTest', () => {
  let networkPassword: NetworkPasswordInputElement;

  setup(() => {
    networkPassword = document.createElement('network-password-input');
    document.body.appendChild(networkPassword);
    flush();
  });

  test('Show password button', () => {
    const passwordInput =
        networkPassword.shadowRoot!.querySelector<CrInputElement>('#input');
    assertTrue(!!passwordInput);
    assertFalse(networkPassword.showPassword);
    assertEquals('password', passwordInput.type);

    const showPassword =
        networkPassword.shadowRoot!.querySelector<CrIconButtonElement>('#icon');
    assertTrue(!!showPassword);
    showPassword.click();

    assertTrue(networkPassword.showPassword);
    assertEquals('text', passwordInput.type);
  });

  test('Show password button is hidden', () => {
    networkPassword.disabled = true;
    flush();
    assertFalse(
        !!networkPassword.shadowRoot!.querySelector<CrIconButtonElement>(
            '#icon'));

    networkPassword.disabled = false;
    flush();
    assertTrue(!!networkPassword.shadowRoot!.querySelector<CrIconButtonElement>(
        '#icon'));

    const managedPropertiesNone = {
      source: OncSource.kNone,
    } as ManagedProperties;
    networkPassword.managedProperties = managedPropertiesNone;
    flush();
    assertTrue(!!networkPassword.shadowRoot!.querySelector<CrIconButtonElement>(
        '#icon'));

    const managedPropertiesDevice = {
      source: OncSource.kDevice,
    } as ManagedProperties;
    networkPassword.managedProperties = managedPropertiesDevice;
    flush();
    // TODO(crbug.com/328633844): Update this test to check the visibility of
    // the "show password" button when the network type is Wi-Fi.
    assertFalse(
        !!networkPassword.shadowRoot!.querySelector<CrIconButtonElement>(
            '#icon'));
  });

  test('Aria label', () => {
    networkPassword.ariaLabel = 'test_aria_label';
    const passwordInput =
        networkPassword.shadowRoot!.querySelector<CrInputElement>('#input');
    assertTrue(!!passwordInput);

    assertEquals('test_aria_label', passwordInput.ariaLabel);
  });

  test('Clear placeholder password on click', () => {
    const passwordInput =
        networkPassword.shadowRoot!.querySelector<CrInputElement>('#input');
    assertTrue(!!passwordInput);

    passwordInput.value = FAKE_CREDENTIAL;
    passwordInput.dispatchEvent(new MouseEvent('mousedown'));
    assertEquals('', passwordInput.value);

    passwordInput.value = FAKE_CREDENTIAL;
    passwordInput.dispatchEvent(new MouseEvent('touchstart'));
    assertEquals('', passwordInput.value);

    // Verify that clicking the input while not showing the placeholder does not
    // delete the password.
    const newPassword = 'new password';
    passwordInput.value = newPassword;
    passwordInput.dispatchEvent(new MouseEvent('mousedown'));
    assertEquals(newPassword, passwordInput.value);
  });
});
