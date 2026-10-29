// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {CredentialFieldElement} from 'chrome://password-manager/password_manager.js';
import {Page, PasswordManagerImpl, PasswordViewPageInteractions, Router} from 'chrome://password-manager/password_manager.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

const LABEL = 'Username';
const COPY_BUTTON_LABEL = 'Copy username';
const VALUE_COPIED_TOAST_LABEL = 'Username copied!';
const VALUE = 'nadeshiko@example.com';
const INTERACTION_ID =
    PasswordViewPageInteractions.USERNAME_COPY_BUTTON_CLICKED;

function createCredentialFieldElement(): CredentialFieldElement {
  const element = document.createElement('credential-field');
  element.label = LABEL;
  element.copyButtonLabel = COPY_BUTTON_LABEL;
  element.valueCopiedToastLabel = VALUE_COPIED_TOAST_LABEL;
  element.value = VALUE;
  element.interactionId = INTERACTION_ID;
  document.body.appendChild(element);
  return element;
}

suite('CredentialFieldTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    Router.getInstance().navigateTo(Page.PASSWORDS);
  });

  test('element sets all the attributes', function() {
    const element = createCredentialFieldElement();
    assertEquals(element.$.inputValue.value, VALUE);
    assertEquals(element.$.inputValue.label, LABEL);
    assertEquals(element.$.copyButton.title, COPY_BUTTON_LABEL);
  });

  test('copy value', async function() {
    const element = createCredentialFieldElement();

    element.$.copyButton.click();
    await passwordManager.whenCalled('extendAuthValidity');
    assertEquals(
        INTERACTION_ID,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));
  });
});
