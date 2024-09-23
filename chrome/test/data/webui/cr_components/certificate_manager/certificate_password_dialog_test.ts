// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/certificate_manager/certificate_password_dialog.js';
import 'chrome://certificate-manager/strings.m.js';

import type {CertificatePasswordDialogElement} from 'chrome://resources/cr_components/certificate_manager/certificate_password_dialog.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';


suite('CertificatePasswordDialogTest', () => {
  let passwordDialog: CertificatePasswordDialogElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordDialog = document.createElement('certificate-password-dialog');
    document.body.appendChild(passwordDialog);
  });

  test('click ok', async () => {
    // Dialog should automatically open upon being added to DOM.
    assertTrue(passwordDialog.$.dialog.open);
    assertEquals('', passwordDialog.$.password.value);
    assertNull(passwordDialog.value());
    assertFalse(passwordDialog.wasConfirmed());

    passwordDialog.$.password.value = 'something secret';
    // Even though text was entered into the password input, value() should
    // still be null since the dialog hasn't been confirmed yet.
    assertNull(passwordDialog.value());
    assertFalse(passwordDialog.wasConfirmed());

    passwordDialog.$.ok.click();
    assertFalse(passwordDialog.$.dialog.open);
    assertTrue(passwordDialog.wasConfirmed());
    assertEquals('something secret', passwordDialog.$.password.value);
    assertEquals('something secret', passwordDialog.value());
  });

  test('click cancel', async () => {
    passwordDialog.$.password.value = 'something secret';
    passwordDialog.$.cancel.click();
    assertFalse(passwordDialog.$.dialog.open);
    // value in the password field should still be what was entered, but value()
    // returns null since the dialog was cancelled.
    assertEquals('something secret', passwordDialog.$.password.value);
    assertFalse(passwordDialog.wasConfirmed());
    assertNull(passwordDialog.value());
  });
});
