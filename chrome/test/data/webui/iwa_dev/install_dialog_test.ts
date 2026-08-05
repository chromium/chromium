// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/install_dialog.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {IwaDevInstallDialogElement} from 'chrome://iwa-dev/install_dialog.js';
import {TabIndex} from 'chrome://iwa-dev/install_dialog.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('<iwa-dev-install-dialog>', () => {
  let dialog: IwaDevInstallDialogElement;
  let installButton: HTMLButtonElement;
  let cancelButton: HTMLButtonElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('iwa-dev-install-dialog');
    document.body.appendChild(dialog);

    dialog.showDialog();
    await microtasksFinished();

    installButton =
        dialog.shadowRoot.querySelector<HTMLButtonElement>('.action-button')!;
    assertTrue(!!installButton);

    cancelButton =
        dialog.shadowRoot.querySelector<HTMLButtonElement>('.cancel-button')!;
    assertTrue(!!cancelButton);
  });

  test('displays error message on installation failure', async () => {
    dialog.startInstallation();
    await microtasksFinished();

    assertTrue(installButton.hasAttribute('disabled'));
    assertEquals('Installing...', installButton.textContent.trim());

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

    cancelButton.click();
    await microtasksFinished();

    assertFalse(crDialog.open);
  });

  suite('Dev Proxy Tab', () => {
    let proxyTab: HTMLElement;
    let input: CrInputElement;

    setup(() => {
      proxyTab =
          dialog.shadowRoot.querySelector('iwa-dev-install-dev-proxy-tab')!;
      assertTrue(!!proxyTab);

      input = proxyTab.shadowRoot!.querySelector<CrInputElement>('cr-input')!;
      assertTrue(!!input);
    });

    test('validates proxy url on install click', async () => {
      const invalidUrl = 'invalid-url';
      input.value = invalidUrl;
      input.dispatchEvent(
          new CustomEvent('value-changed', {detail: {value: invalidUrl}}));
      await microtasksFinished();

      let eventFired = false;
      dialog.addEventListener('request-install-from-dev-proxy', () => {
        eventFired = true;
      });

      installButton.click();
      await microtasksFinished();

      // Dialog should show error and NOT emit the install event.
      assertTrue(input.invalid);
      assertEquals('Please enter a valid URL.', input.errorMessage);
      assertFalse(eventFired);
    });

    test('emits install event for valid proxy url', async () => {
      const eventPromise =
          eventToPromise('request-install-from-dev-proxy', dialog);

      const validUrl = 'http://localhost:8080';
      input.value = validUrl;
      input.dispatchEvent(
          new CustomEvent('value-changed', {detail: {value: validUrl}}));
      await microtasksFinished();

      installButton.click();

      const e = await eventPromise as CustomEvent<{url: string}>;

      assertFalse(input.invalid);
      assertEquals(validUrl, e.detail.url);
    });

    test(
        'emits install event on Enter keydown in proxy url input', async () => {
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

    test('clears installation error when tab input changes', async () => {
      const errorMessage = 'Failed to fetch web bundle.';
      dialog.onInstallationFinished(errorMessage);
      await microtasksFinished();

      let errorDiv =
          dialog.shadowRoot.querySelector<HTMLElement>('#error-message');
      assertTrue(!!errorDiv);

      const newUrl = 'http://localhost:8080';
      input.value = newUrl;
      input.dispatchEvent(
          new CustomEvent('value-changed', {detail: {value: newUrl}}));
      await microtasksFinished();

      errorDiv = dialog.shadowRoot.querySelector<HTMLElement>('#error-message');
      assertFalse(!!errorDiv);
    });
  });

  suite('Local Bundle Tab', () => {
    let bundleTab: HTMLElement;

    setup(async () => {
      const tabs = dialog.shadowRoot.querySelector('cr-tabs');
      assertTrue(!!tabs);

      tabs.selected = TabIndex.LOCAL_BUNDLE;
      tabs.dispatchEvent(new CustomEvent(
          'selected-changed', {detail: {value: TabIndex.LOCAL_BUNDLE}}));
      await microtasksFinished();

      bundleTab =
          dialog.shadowRoot.querySelector('iwa-dev-install-local-bundle-tab')!;
      assertTrue(!!bundleTab);
    });

    test('enables install button by default', () => {
      assertFalse(installButton.hasAttribute('disabled'));
    });

    test('emits local bundle install event on install click', async () => {
      const eventPromise =
          eventToPromise('request-install-from-local-bundle', dialog);

      installButton.click();

      await eventPromise;
    });
  });
});
