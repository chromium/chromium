// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {CheckupListItemElement} from 'chrome://password-manager/password_manager.js';
import {PasswordAutomaticChangeState} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createCredentialGroup, makeInsecureCredential} from './test_util.js';

suite('CheckupListItemTest', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    return flushTasks();
  });

  async function createCheckupListItem(
      passwordChangeState: PasswordAutomaticChangeState =
          PasswordAutomaticChangeState.kInactive):
      Promise<CheckupListItemElement> {
    const item = makeInsecureCredential({
      id: 42,
      url: 'test.com',
      username: 'viking',
      isAutomaticPasswordChangeSupported: true,
    });
    const group = createCredentialGroup({
      name: 'test.com',
      credentials: [item],
    });

    const element = document.createElement('checkup-list-item');
    element.item = item;
    element.group = group;
    element.passwordChangeState = passwordChangeState;
    document.body.appendChild(element);
    await flushTasks();
    return element;
  }

  test('inactive state shows action button and hides status text', async function() {
    const element =
        await createCheckupListItem(PasswordAutomaticChangeState.kInactive);

    // When inactive, the button should be visible/idle and no status text
    // should be shown.
    const autoChangeButton = element.shadowRoot!.querySelector<HTMLElement>(
        '#autoChangePasswordButton');
    assertTrue(!!autoChangeButton);
    assertTrue(isVisible(autoChangeButton));

    const statusTextSpan = element.shadowRoot!.querySelector<HTMLElement>(
        'span.cr-secondary-text');
    assertFalse(isVisible(statusTextSpan));
  });

  test(
      'changing state hides button and shows changing status',
      async function() {
        const element = await createCheckupListItem(
            PasswordAutomaticChangeState.kChangingPassword);

        // Button should be hidden/not rendered since it's not idle.
        const autoChangeButton = element.shadowRoot!.querySelector<HTMLElement>(
            '#autoChangePasswordButton');
        assertFalse(isVisible(autoChangeButton));

        // Status text should be shown.
        const statusTextSpan = element.shadowRoot!.querySelector<HTMLElement>(
            'span.cr-secondary-text');
        assertTrue(!!statusTextSpan);
        assertTrue(isVisible(statusTextSpan));

        // The text should correspond to
        // 'automatedPasswordChangeChangingPassword'.
        const icon = statusTextSpan.querySelector('cr-icon');
        assertTrue(!!icon);
        assertEquals('passwords-icon:arrow-selector-spark', icon.icon);
        assertEquals(
            loadTimeData.getString('automatedPasswordChangeChangingPassword'),
            statusTextSpan.textContent.trim());
      });

  test('success state shows success icon and text', async function() {
    const element = await createCheckupListItem(
        PasswordAutomaticChangeState.kPasswordChangedSuccessfully);

    const autoChangeButton = element.shadowRoot!.querySelector<HTMLElement>(
        '#autoChangePasswordButton');
    assertFalse(isVisible(autoChangeButton));

    const statusTextSpan = element.shadowRoot!.querySelector<HTMLElement>(
        'span.cr-secondary-text');
    assertTrue(!!statusTextSpan);
    assertTrue(isVisible(statusTextSpan));

    // Icon should be 'passwords-icon:task-spark' for success.
    const icon = statusTextSpan.querySelector('cr-icon');
    assertTrue(!!icon);
    assertEquals('passwords-icon:task-spark', icon.icon);
    assertEquals(
        loadTimeData.getString('automatedPasswordChangeChangedSuccessfully'),
        statusTextSpan.textContent.trim());
  });

  test('error state shows error status', async function() {
    const element =
        await createCheckupListItem(PasswordAutomaticChangeState.kError);

    const autoChangeButton = element.shadowRoot!.querySelector<HTMLElement>(
        '#autoChangePasswordButton');
    assertFalse(isVisible(autoChangeButton));

    const statusTextSpan = element.shadowRoot!.querySelector<HTMLElement>(
        'span.cr-secondary-text');
    assertTrue(!!statusTextSpan);
    assertTrue(isVisible(statusTextSpan));

    const icon = statusTextSpan.querySelector('cr-icon');
    assertTrue(!!icon);
    assertEquals('passwords-icon:arrow-selector-spark', icon.icon);
    assertEquals(
        loadTimeData.getString('automatedPasswordChangeError'),
        statusTextSpan.textContent.trim());
  });
});
