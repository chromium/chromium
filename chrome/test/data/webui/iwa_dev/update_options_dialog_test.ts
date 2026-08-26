// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/update_options_dialog.js';

import type {ChannelMetadata, IwaDevModeAppInfo, UpdateManifest, VersionEntry} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
import type {IwaDevUpdateOptionsDialogElement} from 'chrome://iwa-dev/update_options_dialog.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('<iwa-dev-update-options-dialog>', () => {
  let dialog: IwaDevUpdateOptionsDialogElement;
  let saveButton: HTMLButtonElement;
  let cancelButton: HTMLButtonElement;
  let channelInput: HTMLInputElement;
  let pinnedVersionInput: HTMLInputElement;

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

  async function createDialog(
      app: IwaDevModeAppInfo, currentPinnedVersion: string|null = null) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('iwa-dev-update-options-dialog');
    dialog.app = app;
    dialog.currentPinnedVersion = currentPinnedVersion;

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
    pinnedVersionInput = dialog.shadowRoot.querySelector<HTMLInputElement>(
        '#pinnedVersionInput')!;
    assertTrue(!!pinnedVersionInput);

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

  function getPinnedVersionOptions(): NodeListOf<HTMLOptionElement> {
    return dialog.shadowRoot.querySelectorAll<HTMLOptionElement>(
        '#pinnedVersionList option');
  }

  interface OpenDialogOptions {
    currentChannel?: string;
    currentPinnedVersion?: string|null;
    channels?: ChannelMetadata[];
    versions?: VersionEntry[];
  }

  async function openDialog(options: OpenDialogOptions = {}) {
    const currentChannel = options.currentChannel || 'default';
    const currentPinnedVersion = options.currentPinnedVersion ?? null;
    const channels =
        options.channels || [{channel: 'beta', displayName: 'Beta'}];
    const versions = options.versions || [];

    const callback =
        await createDialog(createAppInfo(currentChannel), currentPinnedVersion);
    callback({success: {versions, channels}});
    await microtasksFinished();
  }

  interface OpenDialogWithErrorOptions {
    currentChannel?: string;
    currentPinnedVersion?: string|null;
    error?: string;
  }

  async function openDialogWithError(options: OpenDialogWithErrorOptions = {}) {
    const currentChannel = options.currentChannel || 'default';
    const currentPinnedVersion = options.currentPinnedVersion ?? null;
    const error = options.error || 'Network error';

    const callback =
        await createDialog(createAppInfo(currentChannel), currentPinnedVersion);
    callback({error});
    await microtasksFinished();
  }

  test(
      'fetches manifest on open, pre-fills current channel, and populates ' +
          'datalists with all options',
      async () => {
        await openDialog({
          currentChannel: 'default',
          currentPinnedVersion: '1.0.0',
          channels: [
            {channel: 'default', displayName: 'Default'},
            {channel: 'beta', displayName: 'Beta Channel'},
            {channel: 'canary', displayName: 'Canary'},
          ],
          versions: [
            {version: '1.0.0', src: '', channels: ['default']},
            {version: '2.0.0', src: '', channels: ['beta']},
            {version: '3.0.0', src: '', channels: ['canary']},
          ],
        });

        assertTrue(dialog.$.dialog.open);
        assertEquals('default', channelInput.value);
        assertEquals('1.0.0', pinnedVersionInput.value);

        const channelOptions = getChannelOptions();
        assertEquals(3, channelOptions.length);
        assertEquals('default', channelOptions[0]!.value);
        assertEquals('Default', channelOptions[0]!.textContent?.trim());
        assertEquals('beta', channelOptions[1]!.value);
        assertEquals('Beta Channel', channelOptions[1]!.textContent?.trim());
        assertEquals('canary', channelOptions[2]!.value);
        assertEquals('Canary', channelOptions[2]!.textContent?.trim());

        const versionOptions = getPinnedVersionOptions();
        assertEquals(3, versionOptions.length);
        assertEquals('1.0.0', versionOptions[0]!.value);
        assertEquals('2.0.0', versionOptions[1]!.value);
        assertEquals('3.0.0', versionOptions[2]!.value);

        assertTrue(saveButton.hasAttribute('disabled'));
      });

  test(
      'displays error message and allows saving custom values when manifest ' +
          'fetch fails',
      async () => {
        await openDialogWithError({
          currentChannel: 'default',
          error: 'Failed to fetch manifest',
        });

        const errorDiv =
            dialog.shadowRoot.querySelector<HTMLElement>('.error-message');
        assertTrue(!!errorDiv);
        assertEquals(
            'Failed to fetch suggestions from update manifest.',
            errorDiv.textContent?.trim());
        assertTrue(saveButton.hasAttribute('disabled'));

        channelInput.value = 'beta';
        channelInput.dispatchEvent(new Event('input'));
        pinnedVersionInput.value = '2.0.0';
        pinnedVersionInput.dispatchEvent(new Event('input'));
        await microtasksFinished();

        assertFalse(saveButton.hasAttribute('disabled'));

        const savePromise = eventToPromise('update-options-saved', dialog);
        saveButton.click();

        const saveEvent = await savePromise as CustomEvent<{
                            selectedChannel: string,
                            pinnedVersion: string,
                          }>;
        assertEquals('beta', saveEvent.detail.selectedChannel);
        assertEquals('2.0.0', saveEvent.detail.pinnedVersion);
        assertFalse(dialog.$.dialog.open);
      });

  test(
      'disables save button until a different valid channel is entered',
      async () => {
        await openDialog({
          currentChannel: 'default',
          channels: [{channel: 'beta', displayName: 'Beta'}],
        });
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
      'disables save button until a different pinned version is entered',
      async () => {
        await openDialog({
          currentPinnedVersion: '1.0.0',
          versions: [{version: '2.0.0', src: '', channels: ['default']}],
        });
        assertEquals('1.0.0', pinnedVersionInput.value);
        assertTrue(saveButton.hasAttribute('disabled'));

        // Entering current pinned version keeps save disabled
        pinnedVersionInput.value = '1.0.0';
        pinnedVersionInput.dispatchEvent(new Event('input'));
        await microtasksFinished();
        assertTrue(saveButton.hasAttribute('disabled'));

        // Entering a new pinned version enables save
        pinnedVersionInput.value = '2.0.0';
        pinnedVersionInput.dispatchEvent(new Event('input'));
        await microtasksFinished();
        assertFalse(saveButton.hasAttribute('disabled'));

        // Resetting pinned version to current disables save again
        pinnedVersionInput.value = '1.0.0';
        pinnedVersionInput.dispatchEvent(new Event('input'));
        await microtasksFinished();
        assertTrue(saveButton.hasAttribute('disabled'));

        // Clearing pinned version (unpinning) enables save
        pinnedVersionInput.value = '';
        pinnedVersionInput.dispatchEvent(new Event('input'));
        await microtasksFinished();
        assertFalse(saveButton.hasAttribute('disabled'));

        // Resetting to current version disables save again
        pinnedVersionInput.value = '1.0.0';
        pinnedVersionInput.dispatchEvent(new Event('input'));
        await microtasksFinished();
        assertTrue(saveButton.hasAttribute('disabled'));
      });

  test('emits update-options-saved on save click', async () => {
    await openDialog({
      currentChannel: 'default',
      currentPinnedVersion: '1.0.0',
      channels: [{channel: 'beta', displayName: 'Beta'}],
      versions: [{version: '2.0.0', src: '', channels: ['beta']}],
    });

    channelInput.value = '  beta  ';
    channelInput.dispatchEvent(new Event('input'));
    pinnedVersionInput.value = '  2.0.0  ';
    pinnedVersionInput.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const savePromise = eventToPromise('update-options-saved', dialog);
    saveButton.click();

    const saveEvent = await savePromise as CustomEvent<{
                        app: IwaDevModeAppInfo,
                        selectedChannel?: string,
                        pinnedVersion?: string,
                      }>;
    assertEquals('test-app-id', saveEvent.detail.app.appId);
    assertEquals('beta', saveEvent.detail.selectedChannel);
    assertEquals('2.0.0', saveEvent.detail.pinnedVersion);
    assertFalse(dialog.$.dialog.open);
  });

  test('emits null pinned version when input is cleared', async () => {
    await openDialog({
      currentChannel: 'default',
      currentPinnedVersion: '1.0.0',
      channels: [{channel: 'beta', displayName: 'Beta'}],
    });

    pinnedVersionInput.value = '';
    pinnedVersionInput.dispatchEvent(new Event('input'));
    await microtasksFinished();
    assertFalse(saveButton.hasAttribute('disabled'));

    const savePromise = eventToPromise('update-options-saved', dialog);
    saveButton.click();

    const saveEvent = await savePromise as CustomEvent<{
                        pinnedVersion?: string | null,
                      }>;
    assertEquals(null, saveEvent.detail.pinnedVersion);
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
    pinnedVersionInput = dialog.shadowRoot.querySelector<HTMLInputElement>(
        '#pinnedVersionInput')!;

    assertEquals('Loading channels...', channelInput.placeholder);
    assertEquals('Loading versions...', pinnedVersionInput.placeholder);

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
    assertEquals('Select or enter version', pinnedVersionInput.placeholder);
  });

  test('allows saving custom values not present in manifest', async () => {
    await openDialog({
      currentChannel: 'default',
      channels: [{channel: 'default', displayName: 'Default'}],
      versions: [{version: '1.0.0', src: '', channels: ['default']}],
    });

    channelInput.value = 'custom-channel';
    channelInput.dispatchEvent(new Event('input'));
    pinnedVersionInput.value = '99.0.0';
    pinnedVersionInput.dispatchEvent(new Event('input'));
    await microtasksFinished();

    assertFalse(saveButton.hasAttribute('disabled'));

    const savePromise = eventToPromise('update-options-saved', dialog);
    saveButton.click();

    const saveEvent = await savePromise as CustomEvent<{
                        selectedChannel?: string,
                        pinnedVersion?: string,
                      }>;
    assertEquals('custom-channel', saveEvent.detail.selectedChannel);
    assertEquals('99.0.0', saveEvent.detail.pinnedVersion);
  });

  test('ignores manifest fetch callback if dialog is closed', async () => {
    const callback = await createDialog(createAppInfo('default'));

    cancelButton.click();
    await microtasksFinished();
    assertFalse(dialog.$.dialog.open);

    callback({
      success: {
        versions: [{version: '1.0.0', src: '', channels: ['beta']}],
        channels: [{channel: 'beta', displayName: 'Beta'}],
      },
    });
    await microtasksFinished();

    assertEquals(0, getChannelOptions().length);
    assertEquals(0, getPinnedVersionOptions().length);
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
        versions: [{version: '1.0.0', src: '', channels: ['app-1-channel']}],
        channels: [{channel: 'app-1-channel', displayName: 'App 1'}],
      },
    });
    await microtasksFinished();
    assertEquals(0, getChannelOptions().length);
    assertEquals(0, getPinnedVersionOptions().length);

    // Active callback for app 2 arrives
    callback2({
      success: {
        versions: [{version: '2.0.0', src: '', channels: ['app-2-channel']}],
        channels: [{channel: 'app-2-channel', displayName: 'App 2'}],
      },
    });
    await microtasksFinished();

    const channelOptions = getChannelOptions();
    assertEquals(1, channelOptions.length);
    assertEquals('app-2-channel', channelOptions[0]!.value);
    const versionOptions = getPinnedVersionOptions();
    assertEquals(1, versionOptions.length);
    assertEquals('2.0.0', versionOptions[0]!.value);
  });
});
