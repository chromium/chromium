// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/update_options_dialog.js';

import type {ChannelMetadata, IwaDevModeAppInfo, UpdateManifest} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
import type {IwaDevUpdateOptionsDialogElement} from 'chrome://iwa-dev/update_options_dialog.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('<iwa-dev-update-options-dialog>', () => {
  let dialog: IwaDevUpdateOptionsDialogElement;
  let saveButton: HTMLButtonElement;
  let cancelButton: HTMLButtonElement;
  let channelInput: HTMLInputElement;

  function createAppInfo(updateChannel: string = 'default'): IwaDevModeAppInfo {
    return {
      appId: 'test-app-id',
      webBundleId: 'test-bundle-id',
      name: 'Test App',
      installedVersion: '1.0.0',
      source: {
        updateInfo: {
          updateManifestUrl: 'https://example.com/manifest.json',
          updateChannel: updateChannel,
        },
      } as unknown as IwaDevModeAppInfo['source'],
    };
  }

  async function createDialog(app: IwaDevModeAppInfo) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('iwa-dev-update-options-dialog');
    dialog.app = app;

    const fetchPromise =
        eventToPromise('request-parse-update-manifest-from-url', dialog);
    document.body.appendChild(dialog);
    await microtasksFinished();

    saveButton =
        dialog.shadowRoot.querySelector<HTMLButtonElement>('.action-button')!;
    assertTrue(!!saveButton);
    cancelButton =
        dialog.shadowRoot.querySelector<HTMLButtonElement>('.cancel-button')!;
    assertTrue(!!cancelButton);
    channelInput =
        dialog.shadowRoot.querySelector<HTMLInputElement>('#channelInput')!;
    assertTrue(!!channelInput);

    const event = await fetchPromise as CustomEvent<{
                    url: string,
                    callback: (result: {
                      success?: UpdateManifest,
                      error?: string,
                    }) => void,
                  }>;
    return event.detail.callback;
  }

  function getChannelOptions(): NodeListOf<HTMLOptionElement> {
    return dialog.shadowRoot.querySelectorAll<HTMLOptionElement>(
        '#channelList option');
  }

  async function openDialog(
      currentChannel: string = 'default',
      channels: ChannelMetadata[] = [{channel: 'beta', displayName: 'Beta'}]) {
    const callback = await createDialog(createAppInfo(currentChannel));
    callback({success: {versions: [], channels}});
    await microtasksFinished();
  }

  async function openDialogWithError(
      currentChannel: string = 'default', error: string = 'Network error') {
    const callback = await createDialog(createAppInfo(currentChannel));
    callback({error});
    await microtasksFinished();
  }

  test(
      'fetches manifest on open, pre-fills current channel, and populates ' +
          'datalist with all channels',
      async () => {
        await openDialog('default', [
          {channel: 'default', displayName: 'Default'},
          {channel: 'beta', displayName: 'Beta Channel'},
          {channel: 'canary', displayName: 'Canary'},
        ]);

        assertTrue(dialog.$.dialog.open);
        assertEquals('default', channelInput.value);

        const options = getChannelOptions();
        assertEquals(3, options.length);
        assertEquals('default', options[0]!.value);
        assertEquals('Default', options[0]!.textContent?.trim());
        assertEquals('beta', options[1]!.value);
        assertEquals('Beta Channel', options[1]!.textContent?.trim());
        assertEquals('canary', options[2]!.value);
        assertEquals('Canary', options[2]!.textContent?.trim());
        assertTrue(saveButton.hasAttribute('disabled'));
      });

  test(
      'displays error message and allows saving custom channel when manifest ' +
          'fetch fails',
      async () => {
        await openDialogWithError('default', 'Failed to fetch manifest');

        const errorDiv =
            dialog.shadowRoot.querySelector<HTMLElement>('.error-message');
        assertTrue(!!errorDiv);
        assertEquals(
            'Failed to fetch suggestions from update manifest.',
            errorDiv.textContent?.trim());
        assertTrue(saveButton.hasAttribute('disabled'));

        channelInput.value = 'beta';
        channelInput.dispatchEvent(new Event('input'));
        await microtasksFinished();

        assertFalse(saveButton.hasAttribute('disabled'));

        const savePromise = eventToPromise('update-options-saved', dialog);
        saveButton.click();

        const saveEvent = await savePromise as CustomEvent<{
                            selectedChannel: string,
                          }>;
        assertEquals('beta', saveEvent.detail.selectedChannel);
        assertFalse(dialog.$.dialog.open);
      });

  test(
      'disables save button until a different valid channel is entered',
      async () => {
        await openDialog('default', [{channel: 'beta', displayName: 'Beta'}]);
        assertEquals('default', channelInput.value);
        assertTrue(saveButton.hasAttribute('disabled'));

        // Clearing input keeps save disabled
        channelInput.value = '';
        channelInput.dispatchEvent(new Event('input'));
        await microtasksFinished();
        assertTrue(saveButton.hasAttribute('disabled'));

        // Entering whitespace keeps save disabled
        channelInput.value = '   ';
        channelInput.dispatchEvent(new Event('input'));
        await microtasksFinished();
        assertTrue(saveButton.hasAttribute('disabled'));

        // Entering a new channel enables save
        channelInput.value = 'beta';
        channelInput.dispatchEvent(new Event('input'));
        await microtasksFinished();
        assertFalse(saveButton.hasAttribute('disabled'));

        // Entering current channel disables save again
        channelInput.value = 'default';
        channelInput.dispatchEvent(new Event('input'));
        await microtasksFinished();
        assertTrue(saveButton.hasAttribute('disabled'));
      });

  test(
      'emits update-options-saved on save click and closes dialog',
      async () => {
        await openDialog('default', [{channel: 'beta', displayName: 'Beta'}]);

        channelInput.value = '  beta  ';
        channelInput.dispatchEvent(new Event('input'));
        await microtasksFinished();

        const savePromise = eventToPromise('update-options-saved', dialog);
        saveButton.click();

        const saveEvent = await savePromise as CustomEvent<{
                            app: IwaDevModeAppInfo,
                            selectedChannel: string,
                          }>;
        assertEquals('test-app-id', saveEvent.detail.app.appId);
        assertEquals('beta', saveEvent.detail.selectedChannel);
        assertFalse(dialog.$.dialog.open);
      });

  test('closes dialog on cancel click', async () => {
    await openDialog();

    let saveEventFired = false;
    dialog.addEventListener('update-options-saved', () => {
      saveEventFired = true;
    });

    cancelButton.click();
    await microtasksFinished();

    assertFalse(dialog.$.dialog.open);
    assertFalse(saveEventFired);
  });

  test('shows appropriate placeholder text', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('iwa-dev-update-options-dialog');
    dialog.app = createAppInfo('default');

    const manifestRequestPromise =
        eventToPromise('request-parse-update-manifest-from-url', dialog);
    document.body.appendChild(dialog);
    await microtasksFinished();

    channelInput =
        dialog.shadowRoot.querySelector<HTMLInputElement>('#channelInput')!;

    assertEquals('Loading channels...', channelInput.placeholder);

    const event =
        await manifestRequestPromise as CustomEvent<{
          url: string,
          callback: (result: {success?: UpdateManifest, error?: string}) =>
              void,
        }>;
    event.detail.callback({
      success: {
        versions: [],
        channels: [{channel: 'default', displayName: 'Default'}],
      },
    });
    await microtasksFinished();

    assertEquals('Select or enter channel', channelInput.placeholder);
  });

  test('allows saving a custom channel not present in manifest', async () => {
    await openDialog('default', [
      {channel: 'default', displayName: 'Default'},
    ]);

    channelInput.value = 'custom-channel';
    channelInput.dispatchEvent(new Event('input'));
    await microtasksFinished();

    assertFalse(saveButton.hasAttribute('disabled'));

    const savePromise = eventToPromise('update-options-saved', dialog);
    saveButton.click();

    const saveEvent = await savePromise as CustomEvent<{
                        selectedChannel: string,
                      }>;
    assertEquals('custom-channel', saveEvent.detail.selectedChannel);
  });

  test('ignores manifest fetch callback if dialog is closed', async () => {
    const callback = await createDialog(createAppInfo('default'));

    cancelButton.click();
    await microtasksFinished();
    assertFalse(dialog.$.dialog.open);

    callback({
      success: {
        versions: [],
        channels: [{channel: 'beta', displayName: 'Beta'}],
      },
    });
    await microtasksFinished();

    assertEquals(0, getChannelOptions().length);
  });

  test('ignores fetch callback from previous dialog instance', async () => {
    const callback1 = await createDialog(createAppInfo('default'));
    cancelButton.click();
    await microtasksFinished();

    const callback2 = await createDialog({
      ...createAppInfo('default'),
      appId: 'app-2',
    });

    // Stale callback for app 1 arrives
    callback1({
      success: {
        versions: [],
        channels: [{channel: 'app-1-channel', displayName: 'App 1'}],
      },
    });
    await microtasksFinished();
    assertEquals(0, getChannelOptions().length);

    // Active callback for app 2 arrives
    callback2({
      success: {
        versions: [],
        channels: [{channel: 'app-2-channel', displayName: 'App 2'}],
      },
    });
    await microtasksFinished();

    const channelOptions = getChannelOptions();
    assertEquals(1, channelOptions.length);
    assertEquals('app-2-channel', channelOptions[0]!.value);
  });
});
