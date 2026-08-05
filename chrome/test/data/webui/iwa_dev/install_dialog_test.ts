// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/install_dialog.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {IwaDevInstallDialogElement} from 'chrome://iwa-dev/install_dialog.js';
import {TabIndex} from 'chrome://iwa-dev/install_dialog.js';
import {MIN_FETCH_DELAY_MS, PLACEHOLDER_URL} from 'chrome://iwa-dev/install_update_manifest_tab.js';
import type {UpdateManifest} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
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

  suite('Update Manifest Tab', () => {
    let manifestTab: HTMLElement;
    let input: CrInputElement;
    let fetchButton: HTMLElement;

    setup(async () => {
      const tabs = dialog.shadowRoot.querySelector('cr-tabs');
      assertTrue(!!tabs);

      tabs.selected = TabIndex.UPDATE_MANIFEST;
      tabs.dispatchEvent(new CustomEvent(
          'selected-changed', {detail: {value: TabIndex.UPDATE_MANIFEST}}));
      await microtasksFinished();

      manifestTab = dialog.shadowRoot.querySelector(
          'iwa-dev-install-update-manifest-tab')!;
      assertTrue(!!manifestTab);
      input =
          manifestTab.shadowRoot!.querySelector<CrInputElement>('cr-input')!;
      assertTrue(!!input);

      fetchButton =
          manifestTab.shadowRoot!.querySelector<HTMLElement>('#fetchButton')!;
      assertTrue(!!fetchButton);
    });

    async function fetchUpdateManifest(
        validUrl: string, result: {success?: UpdateManifest, error?: string}) {
      const fetchEventPromise =
          eventToPromise('request-parse-update-manifest-from-url', dialog);

      input.value = validUrl;
      input.dispatchEvent(
          new CustomEvent('value-changed', {detail: {value: validUrl}}));
      await microtasksFinished();

      assertEquals('Fetch', fetchButton.textContent?.trim());
      fetchButton.click();
      const fetchEventResult =
          await fetchEventPromise as CustomEvent<{
            url: string,
            callback: (result: {success?: UpdateManifest, error?: string}) =>
                void,
          }>;

      fetchEventResult.detail.callback(result);
      await new Promise(
          resolve => setTimeout(resolve, MIN_FETCH_DELAY_MS + 10));
      await microtasksFinished();

      return fetchEventResult;
    }

    test('disables install button initially', () => {
      assertTrue(installButton.disabled);
    });

    test(
        'shows error for invalid manifest url format on fetch click',
        async () => {
          const invalidUrl = 'invalid-manifest-url';
          input.value = invalidUrl;
          input.dispatchEvent(
              new CustomEvent('value-changed', {detail: {value: invalidUrl}}));
          await microtasksFinished();

          fetchButton.click();
          await microtasksFinished();

          assertTrue(input.invalid);
          assertEquals('Please enter a valid URL.', input.errorMessage);
        });

    test('emits parse request event for valid manifest url', async () => {
      const eventPromise =
          eventToPromise('request-parse-update-manifest-from-url', dialog);

      const validUrl = 'http://localhost:8080/manifest.json';
      input.value = validUrl;
      input.dispatchEvent(
          new CustomEvent('value-changed', {detail: {value: validUrl}}));
      await microtasksFinished();

      fetchButton.click();
      const e =
          await eventPromise as CustomEvent<{
            url: string,
            callback: (result: {success?: UpdateManifest, error?: string}) =>
                void,
          }>;

      assertFalse(input.invalid);
      assertEquals(validUrl, e.detail.url);
      assertTrue(typeof e.detail.callback === 'function');
    });

    test(
        'emits parse request event on Enter keydown in url input', async () => {
          const eventPromise =
              eventToPromise('request-parse-update-manifest-from-url', dialog);

          const validUrl = 'http://localhost:8080/manifest.json';
          input.value = validUrl;
          input.dispatchEvent(
              new CustomEvent('value-changed', {detail: {value: validUrl}}));
          await microtasksFinished();

          input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

          const e =
              await eventPromise as CustomEvent<{
                url: string,
                callback: (
                    result: {success?: UpdateManifest, error?: string}) => void,
              }>;

          assertFalse(input.invalid);
          assertEquals(validUrl, e.detail.url);
        });

    test(
        'auto-completes placeholder url on Tab keydown when url input is empty',
        async () => {
          assertEquals('', input.value);
          input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Tab'}));
          await microtasksFinished();

          assertEquals(PLACEHOLDER_URL, input.value);
        });

    test('updates UI upon successful manifest fetch', async () => {
      await fetchUpdateManifest('http://localhost:8080/manifest.json', {
        success: {
          versions: [{
            version: '1.0.0',
            src: 'http://localhost/app.swbn',
            channels: ['stable'],
          }],
          channels: [{channel: 'stable', displayName: 'Stable'}],
        },
      });

      const successMsg = manifestTab.shadowRoot!.querySelector<HTMLElement>(
          '#fetchSuccessMessage');
      assertTrue(!!successMsg);
      assertEquals(
          'Manifest loaded successfully: 1 version available.',
          successMsg.textContent?.trim());

      const versionSelect =
          manifestTab.shadowRoot!.querySelector<HTMLSelectElement>(
              '#versionSelect');
      assertTrue(!!versionSelect);
      assertEquals('1.0.0', versionSelect.value);

      const channelSelect =
          manifestTab.shadowRoot!.querySelector<HTMLSelectElement>(
              '#channelSelect');
      assertTrue(!!channelSelect);
      assertEquals('stable', channelSelect.value);

      assertFalse(installButton.disabled);
    });

    test(
        'hides channel dropdown when manifest has empty channels', async () => {
          await fetchUpdateManifest('http://localhost:8080/manifest.json', {
            success: {
              versions: [{
                version: '1.0.0',
                src: 'http://localhost/app.swbn',
                channels: [],
              }],
              channels: [],
            },
          });

          const versionSelect =
              manifestTab.shadowRoot!.querySelector<HTMLSelectElement>(
                  '#versionSelect');
          assertTrue(!!versionSelect);
          assertEquals('1.0.0', versionSelect.value);

          const channelSelect =
              manifestTab.shadowRoot!.querySelector<HTMLSelectElement>(
                  '#channelSelect');
          assertFalse(!!channelSelect);
        });

    test(
        'sorts versions from latest on top and adds (Latest) postfix',
        async () => {
          await fetchUpdateManifest('http://localhost:8080/manifest.json', {
            success: {
              versions: [
                {
                  version: '1.0.0',
                  src: 'http://localhost/app1.swbn',
                  channels: [],
                },
                {
                  version: '2.1.0',
                  src: 'http://localhost/app3.swbn',
                  channels: [],
                },
                {
                  version: '1.5.0',
                  src: 'http://localhost/app2.swbn',
                  channels: [],
                },
              ],
              channels: [],
            },
          });

          const versionSelect =
              manifestTab.shadowRoot!.querySelector<HTMLSelectElement>(
                  '#versionSelect');
          assertTrue(!!versionSelect);

          const options = Array.from(versionSelect.options);
          assertEquals(3, options.length);

          assertEquals('2.1.0', options[0]!.value);
          assertEquals('2.1.0 (Latest)', options[0]!.text.trim());

          assertEquals('1.5.0', options[1]!.value);
          assertEquals('1.5.0', options[1]!.text.trim());

          assertEquals('1.0.0', options[2]!.value);
          assertEquals('1.0.0', options[2]!.text.trim());

          assertEquals('2.1.0', versionSelect.value);
        });

    test(
        'resets manifest fetched state when url changes after successful fetch',
        async () => {
          await fetchUpdateManifest('http://localhost:8080/manifest.json', {
            success: {
              versions: [{
                version: '1.0.0',
                src: 'http://localhost/app.swbn',
                channels: ['stable'],
              }],
              channels: [{channel: 'stable', displayName: 'Stable'}],
            },
          });

          assertFalse(installButton!.disabled);

          // User modifies URL after successful fetch
          const newUrl = 'http://localhost:8080/new-manifest.json';
          input.value = newUrl;
          input.dispatchEvent(
              new CustomEvent('value-changed', {detail: {value: newUrl}}));
          await microtasksFinished();

          // Install button should now be disabled and details hidden
          assertTrue(installButton!.disabled);
          const versionSelect =
              manifestTab.shadowRoot!.querySelector('#versionSelect');
          assertFalse(!!versionSelect);
        });

    test('emits install event for update manifest on submit', async () => {
      const validUrl = 'http://localhost:8080/manifest.json';
      await fetchUpdateManifest(validUrl, {
        success: {
          versions: [{
            version: '1.0.0',
            src: 'http://localhost/app.swbn',
            channels: ['stable'],
          }],
          channels: [{channel: 'stable', displayName: 'Stable'}],
        },
      });

      const installButton =
          dialog.shadowRoot.querySelector<HTMLButtonElement>('.action-button');
      assertTrue(!!installButton);
      assertFalse(installButton.disabled);

      const installEventPromise =
          eventToPromise('request-install-from-update-manifest', dialog);

      installButton.click();

      const installEvent =
          await installEventPromise as CustomEvent<{
            webBundleUrl: string,
            updateInfo: {updateManifestUrl: string, updateChannel: string},
          }>;

      assertEquals(
          'http://localhost/app.swbn', installEvent.detail.webBundleUrl);
      assertEquals(validUrl, installEvent.detail.updateInfo.updateManifestUrl);
      assertEquals('stable', installEvent.detail.updateInfo.updateChannel);
    });

    test(
        'emits install event on Enter keydown in version select dropdown',
        async () => {
          await fetchUpdateManifest('http://localhost:8080/manifest.json', {
            success: {
              versions: [{
                version: '1.0.0',
                src: 'http://localhost/app.swbn',
                channels: ['stable'],
              }],
              channels: [{channel: 'stable', displayName: 'Stable'}],
            },
          });

          const versionSelect =
              manifestTab.shadowRoot!.querySelector<HTMLSelectElement>(
                  '#versionSelect');
          assertTrue(!!versionSelect);

          const installEventPromise =
              eventToPromise('request-install-from-update-manifest', dialog);

          versionSelect.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'Enter'}));

          const installEvent =
              await installEventPromise as CustomEvent<{
                webBundleUrl: string,
                updateInfo: {updateManifestUrl: string, updateChannel: string},
              }>;

          assertEquals(
              'http://localhost/app.swbn', installEvent.detail.webBundleUrl);
        });

    test(
        'emits install event on Enter keydown in channel select dropdown',
        async () => {
          await fetchUpdateManifest('http://localhost:8080/manifest.json', {
            success: {
              versions: [{
                version: '1.0.0',
                src: 'http://localhost/app.swbn',
                channels: ['stable'],
              }],
              channels: [{channel: 'stable', displayName: 'Stable'}],
            },
          });

          const channelSelect =
              manifestTab.shadowRoot!.querySelector<HTMLSelectElement>(
                  '#channelSelect');
          assertTrue(!!channelSelect);

          const installEventPromise =
              eventToPromise('request-install-from-update-manifest', dialog);

          channelSelect.dispatchEvent(
              new KeyboardEvent('keydown', {key: 'Enter'}));

          const installEvent =
              await installEventPromise as CustomEvent<{
                webBundleUrl: string,
                updateInfo: {updateManifestUrl: string, updateChannel: string},
              }>;

          assertEquals(
              'http://localhost/app.swbn', installEvent.detail.webBundleUrl);
        });

    test(
        'displays error message when update manifest fetch fails', async () => {
          const errorMessage = 'Manifest fetch failed: 404 Not Found';
          await fetchUpdateManifest(
              'http://localhost:8080/manifest.json', {error: errorMessage});

          assertTrue(input.invalid);
          assertEquals(errorMessage, input.errorMessage);

          const installButton =
              dialog.shadowRoot.querySelector<HTMLButtonElement>(
                  '.action-button');
          assertTrue(!!installButton);
          assertTrue(installButton.disabled);
        });

    test(
        'displays error message when update manifest has empty versions',
        async () => {
          await fetchUpdateManifest(
              'http://localhost:8080/manifest.json',
              {success: {versions: [], channels: []}});

          assertTrue(input.invalid);
          assertEquals(
              'No valid version entries found in update manifest.',
              input.errorMessage);

          const installButton =
              dialog.shadowRoot.querySelector<HTMLButtonElement>(
                  '.action-button');
          assertTrue(!!installButton);
          assertTrue(installButton.disabled);
        });
  });
});
