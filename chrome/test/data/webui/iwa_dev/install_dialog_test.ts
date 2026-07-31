// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/install_dialog.js';

import type {IwaDevInstallDialogElement} from 'chrome://iwa-dev/install_dialog.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('<iwa-dev-install-dialog>', () => {
  let dialog: IwaDevInstallDialogElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('iwa-dev-install-dialog');
    document.body.appendChild(dialog);

    dialog.showDialog();
    await microtasksFinished();
  });

  test('validates proxy url on install click', async () => {
    const tabs = dialog.shadowRoot.querySelector('cr-tabs');
    assertTrue(!!tabs);
    assertEquals(0, tabs.selected);

    const proxyTab =
        dialog.shadowRoot.querySelector('iwa-dev-install-dev-proxy-tab');
    assertTrue(!!proxyTab);
    const input = proxyTab.shadowRoot.querySelector('cr-input');
    assertTrue(!!input);

    const invalidUrl = 'invalid-url';
    input.value = invalidUrl;
    input.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: invalidUrl}}));
    await microtasksFinished();

    let eventFired = false;
    dialog.addEventListener('request-install-from-dev-proxy', () => {
      eventFired = true;
    });

    const installButton =
        dialog.shadowRoot.querySelector<HTMLElement>('.action-button');
    assertTrue(!!installButton);
    installButton.click();
    await microtasksFinished();

    // Dialog should show error and NOT emit the install event.
    assertTrue(input.invalid);
    assertEquals('Please enter a valid URL.', input.errorMessage);
    assertFalse(eventFired);
  });

  test('emits install event for valid proxy url', async () => {
    const tabs = dialog.shadowRoot.querySelector('cr-tabs');
    assertTrue(!!tabs);
    assertEquals(0, tabs.selected);

    const proxyTab =
        dialog.shadowRoot.querySelector('iwa-dev-install-dev-proxy-tab');
    assertTrue(!!proxyTab);
    const input = proxyTab.shadowRoot.querySelector('cr-input');
    assertTrue(!!input);

    const eventPromise =
        eventToPromise('request-install-from-dev-proxy', dialog);

    const validUrl = 'http://localhost:8080';
    input.value = validUrl;
    input.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: validUrl}}));
    await microtasksFinished();

    const installButton =
        dialog.shadowRoot.querySelector<HTMLElement>('.action-button');
    assertTrue(!!installButton);
    installButton.click();

    const e = await eventPromise as CustomEvent<{url: string}>;

    assertFalse(input.invalid);
    assertEquals(validUrl, e.detail.url);
  });

  test('emits install event on Enter keydown in proxy url input', async () => {
    const proxyTab =
        dialog.shadowRoot.querySelector('iwa-dev-install-dev-proxy-tab');
    assertTrue(!!proxyTab);
    const input = proxyTab.shadowRoot.querySelector('cr-input');
    assertTrue(!!input);

    const eventPromise =
        eventToPromise('request-install-from-dev-proxy', dialog);

    const validUrl = 'http://localhost:8080';
    input.value = validUrl;
    input.dispatchEvent(
        new CustomEvent('value-changed', {detail: {value: validUrl}}));
    await microtasksFinished();

    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

    const e = await eventPromise as CustomEvent<{url: string}>;
    assertEquals(validUrl, e.detail.url);
  });

  test('displays error message on installation failure', async () => {
    dialog.startInstallation();
    await microtasksFinished();

    const installButton =
        dialog.shadowRoot.querySelector<HTMLElement>('.action-button');
    assertTrue(!!installButton);
    assertTrue(installButton.hasAttribute('disabled'));
    assertEquals('Installing...', installButton.textContent.trim());

    const cancelButton =
        dialog.shadowRoot.querySelector<HTMLElement>('.cancel-button');
    assertTrue(!!cancelButton);
    assertTrue(cancelButton.hasAttribute('disabled'));

    const errorMessage = 'Failed to fetch web bundle.';
    dialog.onInstallationFinished(errorMessage);
    await microtasksFinished();

    const crDialog = dialog.$.dialog;
    assertTrue(!!crDialog);
    assertTrue(crDialog.open);

    const errorDiv =
        dialog.shadowRoot.querySelector<HTMLElement>('#error-message');
    assertTrue(!!errorDiv);
    assertEquals(errorMessage, errorDiv.textContent?.trim());
  });

  test('closes dialog on installation success', async () => {
    dialog.startInstallation();
    await microtasksFinished();

    dialog.onInstallationFinished(/*error=*/ null);
    await microtasksFinished();

    const crDialog = dialog.$.dialog;
    assertTrue(!!crDialog);
    assertFalse(crDialog.open);
    assertFalse(!!dialog.shadowRoot.querySelector('#error-message'));
  });

  test('closes dialog on cancel click', async () => {
    const crDialog = dialog.$.dialog;
    assertTrue(!!crDialog);
    assertTrue(crDialog.open);

    const cancelButton =
        dialog.shadowRoot.querySelector<HTMLElement>('.cancel-button');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await microtasksFinished();

    assertFalse(crDialog.open);
  });
});
