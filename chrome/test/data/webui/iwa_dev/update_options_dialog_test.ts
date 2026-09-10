// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://iwa-dev/update_options_dialog.js';

import type {ChannelMetadata, IwaDevModeAppInfo, UpdateManifest, VersionEntry} from 'chrome://iwa-dev/iwa_dev.mojom-webui.js';
import {isValidIwaVersion, isValidUpdateChannel} from 'chrome://iwa-dev/update_options_dialog.js';
import type {IwaDevUpdateOptionsDialogElement} from 'chrome://iwa-dev/update_options_dialog.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('<iwa-dev-update-options-dialog>', () => {
  let dialog: IwaDevUpdateOptionsDialogElement;
  let saveButton: HTMLButtonElement;
  let cancelButton: HTMLButtonElement;
  let channelInput: HTMLInputElement;
  let pinnedVersionInput: HTMLInputElement;
  let allowDowngradesToggle: CrToggleElement;

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
      app: IwaDevModeAppInfo, currentPinnedVersion: string|null = null,
      currentAllowDowngrades: boolean = false) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('iwa-dev-update-options-dialog');
    dialog.app = app;
    dialog.currentPinnedVersion = currentPinnedVersion;
    dialog.currentAllowDowngrades = currentAllowDowngrades;

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
    channelInput = dialog.$.channelCombobox.inputElement;
    assertTrue(!!channelInput);
    pinnedVersionInput = dialog.$.pinnedVersionCombobox.inputElement;
    assertTrue(!!pinnedVersionInput);
    allowDowngradesToggle = dialog.shadowRoot.querySelector<CrToggleElement>(
        '#allowDowngradesToggle')!;
    assertTrue(!!allowDowngradesToggle);

    const event = await fetchPromise as CustomEvent<{
                    url: string,
                    callback: (result: {
                      success?: UpdateManifest,
                      error?: string,
                    }) => void,
                  }>;
    return event.detail.callback;
  }

  function getChannelOptions(): NodeListOf<HTMLButtonElement> {
    return dialog.$.channelCombobox.shadowRoot
        .querySelectorAll<HTMLButtonElement>('#suggestions .suggestion-item');
  }

  function getPinnedVersionOptions(): NodeListOf<HTMLButtonElement> {
    return dialog.$.pinnedVersionCombobox.shadowRoot
        .querySelectorAll<HTMLButtonElement>('#suggestions .suggestion-item');
  }

  interface OpenDialogOptions {
    currentChannel?: string;
    currentPinnedVersion?: string|null;
    currentAllowDowngrades?: boolean;
    channels?: ChannelMetadata[];
    versions?: VersionEntry[];
  }

  async function openDialog(options: OpenDialogOptions = {}) {
    const currentChannel = options.currentChannel || 'default';
    const currentPinnedVersion = options.currentPinnedVersion ?? null;
    const currentAllowDowngrades = options.currentAllowDowngrades || false;
    const channels =
        options.channels || [{channel: 'beta', displayName: 'Beta'}];
    const versions = options.versions || [];

    const callback = await createDialog(
        createAppInfo(currentChannel), currentPinnedVersion,
        currentAllowDowngrades);
    callback({success: {versions, channels}});
    await microtasksFinished();
  }

  interface OpenDialogWithErrorOptions {
    currentChannel?: string;
    currentPinnedVersion?: string|null;
    currentAllowDowngrades?: boolean;
    error?: string;
  }

  async function openDialogWithError(options: OpenDialogWithErrorOptions = {}) {
    const currentChannel = options.currentChannel || 'default';
    const currentPinnedVersion = options.currentPinnedVersion ?? null;
    const currentAllowDowngrades = options.currentAllowDowngrades || false;
    const error = options.error || 'Network error';

    const callback = await createDialog(
        createAppInfo(currentChannel), currentPinnedVersion,
        currentAllowDowngrades);
    callback({error});
    await microtasksFinished();
  }

  test(
      'fetches manifest on open, pre-fills current channel, and populates ' +
          'comboboxes with all options',
      async () => {
        await openDialog({
          currentChannel: 'default',
          currentPinnedVersion: '1.0.0',
          currentAllowDowngrades: true,
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
        assertTrue(allowDowngradesToggle.checked);

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
        assertEquals('3.0.0', versionOptions[0]!.value);
        assertEquals('2.0.0', versionOptions[1]!.value);
        assertEquals('1.0.0', versionOptions[2]!.value);

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
      'disables save button until a different channel is entered', async () => {
        await openDialog({
          currentChannel: 'default',
          channels: [{channel: 'beta', displayName: 'Beta'}],
        });
        assertEquals('default', channelInput.value);
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

  test('disables save button until allow downgrades is toggled', async () => {
    await openDialog({currentAllowDowngrades: false});
    assertFalse(allowDowngradesToggle.checked);
    assertTrue(saveButton.hasAttribute('disabled'));

    // Toggling on enables save
    allowDowngradesToggle.click();
    await microtasksFinished();
    assertTrue(allowDowngradesToggle.checked);
    assertFalse(saveButton.hasAttribute('disabled'));

    // Toggling off disables save again
    allowDowngradesToggle.click();
    await microtasksFinished();
    assertFalse(allowDowngradesToggle.checked);
    assertTrue(saveButton.hasAttribute('disabled'));
  });

  test('emits update-options-saved on save click', async () => {
    await openDialog({
      currentChannel: 'default',
      currentPinnedVersion: '1.0.0',
      currentAllowDowngrades: false,
      channels: [{channel: 'beta', displayName: 'Beta'}],
      versions: [{version: '2.0.0', src: '', channels: ['beta']}],
    });

    channelInput.value = '  beta  ';
    channelInput.dispatchEvent(new Event('input'));
    pinnedVersionInput.value = '  2.0.0  ';
    pinnedVersionInput.dispatchEvent(new Event('input'));
    allowDowngradesToggle.click();
    await microtasksFinished();

    const savePromise = eventToPromise('update-options-saved', dialog);
    saveButton.click();

    const saveEvent = await savePromise as CustomEvent<{
                        app: IwaDevModeAppInfo,
                        selectedChannel?: string,
                        pinnedVersion?: string,
                        allowDowngrades?: boolean,
                      }>;
    assertEquals('test-app-id', saveEvent.detail.app.appId);
    assertEquals('beta', saveEvent.detail.selectedChannel);
    assertEquals('2.0.0', saveEvent.detail.pinnedVersion);
    assertTrue(!!saveEvent.detail.allowDowngrades);
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

  test('clears pinned version when clear button is clicked', async () => {
    await openDialog({
      currentPinnedVersion: '1.0.0',
    });
    assertEquals('1.0.0', pinnedVersionInput.value);

    const clearButton =
        dialog.$.pinnedVersionCombobox.shadowRoot.querySelector<HTMLElement>(
            '#clearButton');
    assertTrue(!!clearButton);
    assertEquals('Clear', clearButton.title);

    clearButton.click();
    await microtasksFinished();

    assertEquals('', pinnedVersionInput.value);
    assertEquals(
        pinnedVersionInput,
        dialog.$.pinnedVersionCombobox.shadowRoot.activeElement);
    assertFalse(saveButton.hasAttribute('disabled'));
    assertFalse(!!dialog.$.pinnedVersionCombobox.shadowRoot.querySelector(
        '#clearButton'));

    const savePromise = eventToPromise('update-options-saved', dialog);
    saveButton.click();

    const saveEvent = await savePromise as CustomEvent<{
                        pinnedVersion?: string | null,
                      }>;
    assertEquals(null, saveEvent.detail.pinnedVersion);
  });

  test('clear pinned version button is not visible when unpinned', async () => {
    await openDialog({
      currentPinnedVersion: null,
    });
    assertEquals('', pinnedVersionInput.value);
    assertFalse(!!dialog.$.pinnedVersionCombobox.shadowRoot.querySelector(
        '#clearButton'));
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
    await openDialog();
    assertEquals('Select or enter channel', channelInput.placeholder);
    assertEquals('Select or enter version', pinnedVersionInput.placeholder);
  });

  test('displays app icon in dialog title', async () => {
    await openDialog();
    const appIcon =
        dialog.shadowRoot.querySelector<HTMLImageElement>('#app-icon');
    assertTrue(!!appIcon);
    assertEquals('chrome://app-icon/test-app-id/20', appIcon.src);
    assertEquals('', appIcon.alt);

    const titleSpan =
        dialog.shadowRoot.querySelector<HTMLSpanElement>('[slot="title"] span');
    assertTrue(!!titleSpan);
    assertEquals('Test App • Update Options', titleSpan.textContent?.trim());
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

  test(
      'displays error and prevents saving when version validation fails',
      async () => {
        await openDialog({currentPinnedVersion: '1.0.0'});
        pinnedVersionInput.value = 'invalid-version';
        pinnedVersionInput.dispatchEvent(new Event('input'));
        await microtasksFinished();

        assertFalse(saveButton.hasAttribute('disabled'));

        let eventFired = false;
        dialog.addEventListener('update-options-saved', () => {
          eventFired = true;
        });

        saveButton.click();
        await microtasksFinished();

        assertFalse(eventFired);
        assertTrue(dialog.$.dialog.open);

        const errorDiv = dialog.$.pinnedVersionCombobox.shadowRoot
                             .querySelector<HTMLElement>('#error');
        assertTrue(!!errorDiv);
        assertEquals('Invalid version format.', errorDiv.textContent?.trim());
        assertEquals('true', pinnedVersionInput.getAttribute('aria-invalid'));
        assertEquals(
            pinnedVersionInput,
            dialog.$.pinnedVersionCombobox.shadowRoot.activeElement);
        assertEquals(
            dialog.$.pinnedVersionCombobox, dialog.shadowRoot.activeElement);
        assertTrue(saveButton.hasAttribute('disabled'));

        const suggestionsDiv = dialog.$.pinnedVersionCombobox.shadowRoot
                                   .querySelector<HTMLElement>('#suggestions');
        if (suggestionsDiv) {
          assertFalse(suggestionsDiv.matches(':popover-open'));
        }

        // Modifying input clears the error
        pinnedVersionInput.value = '2.0.0';
        pinnedVersionInput.dispatchEvent(new Event('input'));
        await microtasksFinished();

        assertFalse(!!dialog.$.pinnedVersionCombobox.shadowRoot.querySelector(
            '#error'));
        assertEquals(null, pinnedVersionInput.getAttribute('aria-invalid'));
        assertFalse(saveButton.hasAttribute('disabled'));

        const savedPromise = eventToPromise('update-options-saved', dialog);
        saveButton.click();
        const savedEvent = await savedPromise as CustomEvent<{
                             pinnedVersion?: string | null,
                           }>;
        assertEquals('2.0.0', savedEvent.detail.pinnedVersion);
        assertFalse(dialog.$.dialog.open);
      });

  test('displays error and prevents saving when channel is empty', async () => {
    await openDialog({currentChannel: 'default'});
    channelInput.value = '   ';
    channelInput.dispatchEvent(new Event('input'));
    await microtasksFinished();

    assertFalse(saveButton.hasAttribute('disabled'));

    let eventFired = false;
    dialog.addEventListener('update-options-saved', () => {
      eventFired = true;
    });

    saveButton.click();
    await microtasksFinished();

    assertFalse(eventFired);
    assertTrue(dialog.$.dialog.open);

    const errorDiv =
        dialog.$.channelCombobox.shadowRoot.querySelector<HTMLElement>(
            '#error');
    assertTrue(!!errorDiv);
    assertEquals('Channel cannot be empty.', errorDiv.textContent?.trim());
    assertEquals('true', channelInput.getAttribute('aria-invalid'));
    assertEquals(
        channelInput, dialog.$.channelCombobox.shadowRoot.activeElement);
    assertEquals(dialog.$.channelCombobox, dialog.shadowRoot.activeElement);
    assertTrue(saveButton.hasAttribute('disabled'));

    const suggestionsDiv =
        dialog.$.channelCombobox.shadowRoot.querySelector<HTMLElement>(
            '#suggestions');
    if (suggestionsDiv) {
      assertFalse(suggestionsDiv.matches(':popover-open'));
    }

    // Modifying input clears the error
    channelInput.value = 'beta';
    channelInput.dispatchEvent(new Event('input'));
    await microtasksFinished();

    assertFalse(!!dialog.$.channelCombobox.shadowRoot.querySelector('#error'));
    assertEquals(null, channelInput.getAttribute('aria-invalid'));
    assertFalse(saveButton.hasAttribute('disabled'));

    const savedPromise = eventToPromise('update-options-saved', dialog);
    saveButton.click();
    const savedEvent = await savedPromise as CustomEvent<{
                         selectedChannel?: string,
                       }>;
    assertEquals('beta', savedEvent.detail.selectedChannel);
    assertFalse(dialog.$.dialog.open);
  });

  test(
      'displays error and prevents saving when channel is not valid UTF-8',
      async () => {
        await openDialog({currentChannel: 'default'});
        channelInput.value = '\uD800';
        channelInput.dispatchEvent(new Event('input'));
        await microtasksFinished();

        assertFalse(saveButton.hasAttribute('disabled'));

        let eventFired = false;
        dialog.addEventListener('update-options-saved', () => {
          eventFired = true;
        });

        saveButton.click();
        await microtasksFinished();

        assertFalse(eventFired);
        assertTrue(dialog.$.dialog.open);

        const errorDiv =
            dialog.$.channelCombobox.shadowRoot.querySelector<HTMLElement>(
                '#error');
        assertTrue(!!errorDiv);
        assertEquals('Invalid channel format.', errorDiv.textContent?.trim());
        assertEquals('true', channelInput.getAttribute('aria-invalid'));
        assertEquals(
            channelInput, dialog.$.channelCombobox.shadowRoot.activeElement);
        assertEquals(dialog.$.channelCombobox, dialog.shadowRoot.activeElement);
        assertTrue(saveButton.hasAttribute('disabled'));

        // Modifying input clears the error
        channelInput.value = 'beta';
        channelInput.dispatchEvent(new Event('input'));
        await microtasksFinished();

        assertFalse(
            !!dialog.$.channelCombobox.shadowRoot.querySelector('#error'));
        assertEquals(null, channelInput.getAttribute('aria-invalid'));
        assertFalse(saveButton.hasAttribute('disabled'));

        const savedPromise = eventToPromise('update-options-saved', dialog);
        saveButton.click();
        const savedEvent = await savedPromise as CustomEvent<{
                             selectedChannel?: string,
                           }>;
        assertEquals('beta', savedEvent.detail.selectedChannel);
        assertFalse(dialog.$.dialog.open);
      });

  test(
      'displays errors and highlights both fields when both are invalid',
      async () => {
        await openDialog({
          currentChannel: 'default',
          currentPinnedVersion: '1.0.0',
        });
        channelInput.value = '   ';
        channelInput.dispatchEvent(new Event('input'));
        pinnedVersionInput.value = 'invalid-version';
        pinnedVersionInput.dispatchEvent(new Event('input'));
        await microtasksFinished();

        assertFalse(saveButton.hasAttribute('disabled'));

        let eventFired = false;
        dialog.addEventListener('update-options-saved', () => {
          eventFired = true;
        });

        saveButton.click();
        await microtasksFinished();

        assertFalse(eventFired);
        assertTrue(dialog.$.dialog.open);

        const channelErrorDiv =
            dialog.$.channelCombobox.shadowRoot.querySelector<HTMLElement>(
                '#error');
        assertTrue(!!channelErrorDiv);
        assertEquals(
            'Channel cannot be empty.', channelErrorDiv.textContent?.trim());
        assertEquals('true', channelInput.getAttribute('aria-invalid'));

        const versionErrorDiv = dialog.$.pinnedVersionCombobox.shadowRoot
                                    .querySelector<HTMLElement>('#error');
        assertTrue(!!versionErrorDiv);
        assertEquals(
            'Invalid version format.', versionErrorDiv.textContent?.trim());
        assertEquals('true', pinnedVersionInput.getAttribute('aria-invalid'));

        // First invalid input in DOM order (channelInput) is focused.
        assertEquals(
            channelInput, dialog.$.channelCombobox.shadowRoot.activeElement);
        assertEquals(dialog.$.channelCombobox, dialog.shadowRoot.activeElement);
        assertTrue(saveButton.hasAttribute('disabled'));
      });

  test('opens pinned version suggestions and selects a version', async () => {
    await openDialog({
      versions: [
        {version: '1.0.0', src: '', channels: ['default']},
        {version: '2.0.0', src: '', channels: ['default']},
      ],
    });

    const suggestionsDiv =
        dialog.$.pinnedVersionCombobox.shadowRoot.querySelector<HTMLElement>(
            '#suggestions')!;
    assertTrue(!!suggestionsDiv);
    assertFalse(suggestionsDiv.matches(':popover-open'));

    const dropdownButton =
        dialog.$.pinnedVersionCombobox.shadowRoot.querySelector<HTMLElement>(
            '#dropdownButton')!;
    assertTrue(!!dropdownButton);

    dropdownButton.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true}));
    await microtasksFinished();

    assertTrue(suggestionsDiv.matches(':popover-open'));

    const versionItems = getPinnedVersionOptions();
    assertEquals(2, versionItems.length);

    versionItems[0]!.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true}));
    await microtasksFinished();

    assertFalse(suggestionsDiv.matches(':popover-open'));
    assertEquals('2.0.0', pinnedVersionInput.value);
  });

  test('filters pinned version suggestions when typing', async () => {
    await openDialog({
      versions: [
        {version: '1.0.0', src: '', channels: ['default']},
        {version: '1.2.0', src: '', channels: ['default']},
        {version: '2.0.0', src: '', channels: ['default']},
        {version: '2.1.0', src: '', channels: ['default']},
      ],
    });

    pinnedVersionInput.value = '2.';
    pinnedVersionInput.dispatchEvent(new Event('input'));
    await microtasksFinished();

    const suggestionsDiv =
        dialog.$.pinnedVersionCombobox.shadowRoot.querySelector<HTMLElement>(
            '#suggestions')!;
    assertTrue(suggestionsDiv.matches(':popover-open'));

    const versionItems = getPinnedVersionOptions();
    assertEquals(2, versionItems.length);
    assertEquals('2.1.0', versionItems[0]!.value);
    assertEquals('2.0.0', versionItems[1]!.value);
  });

  test('opens channel suggestions and selects a channel', async () => {
    await openDialog({
      channels: [
        {channel: 'default', displayName: 'Default'},
        {channel: 'beta', displayName: 'Beta Channel'},
      ],
    });

    const suggestionsDiv =
        dialog.$.channelCombobox.shadowRoot.querySelector<HTMLElement>(
            '#suggestions')!;
    assertTrue(!!suggestionsDiv);
    assertFalse(suggestionsDiv.matches(':popover-open'));

    const dropdownButton =
        dialog.$.channelCombobox.shadowRoot.querySelector<HTMLElement>(
            '#dropdownButton')!;
    assertTrue(!!dropdownButton);

    dropdownButton.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true}));
    await microtasksFinished();

    assertTrue(suggestionsDiv.matches(':popover-open'));

    const channelItems = getChannelOptions();
    assertEquals(2, channelItems.length);

    channelItems[1]!.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true}));
    await microtasksFinished();

    assertFalse(suggestionsDiv.matches(':popover-open'));
    assertEquals('beta', channelInput.value);
  });
});

suite('isValidIwaVersion', () => {
  test('valid versions', () => {
    assertTrue(isValidIwaVersion('1'));
    assertTrue(isValidIwaVersion('0'));
    assertTrue(isValidIwaVersion('1.2'));
    assertTrue(isValidIwaVersion('1.2.3'));
    assertTrue(isValidIwaVersion('1.2.3.4'));
    assertTrue(isValidIwaVersion('0.0.0'));
    assertTrue(isValidIwaVersion('10.20.30'));
    assertTrue(isValidIwaVersion('4294967295'));
    assertTrue(isValidIwaVersion('4294967295.4294967294.4294967293'));
    assertTrue(isValidIwaVersion('1.2.0.3'));
  });

  test('invalid versions', () => {
    assertFalse(isValidIwaVersion(''));
    assertFalse(isValidIwaVersion('  '));
    assertFalse(isValidIwaVersion('1.2.3.4.5'));
    assertFalse(isValidIwaVersion('4294967296'));
    assertFalse(isValidIwaVersion('999994294967295.2.3'));
    assertFalse(isValidIwaVersion('1.-2.3'));
    assertFalse(isValidIwaVersion('1..2.3'));
    assertFalse(isValidIwaVersion('.1'));
    assertFalse(isValidIwaVersion('1.'));
    assertFalse(isValidIwaVersion('1.--2.3'));
    assertFalse(isValidIwaVersion('1.+2.3'));
    assertFalse(isValidIwaVersion('a.2.3'));
    assertFalse(isValidIwaVersion('1.a.3'));
    assertFalse(isValidIwaVersion('1.2.a'));
    assertFalse(isValidIwaVersion('1.2.3-a'));
    assertFalse(isValidIwaVersion('1.2.3+a'));
    assertFalse(isValidIwaVersion('1.2.3-a+a'));
    assertFalse(isValidIwaVersion('01.2.3'));
    assertFalse(isValidIwaVersion('1.02.3'));
    assertFalse(isValidIwaVersion('1.2.03'));
  });
});

suite('isValidUpdateChannel', () => {
  test('valid channels', () => {
    assertTrue(isValidUpdateChannel('default'));
    assertTrue(isValidUpdateChannel('beta'));
    assertTrue(isValidUpdateChannel('stable'));
    assertTrue(isValidUpdateChannel('canary-123'));
    assertTrue(isValidUpdateChannel('channel_🚀'));
  });

  test('invalid channels', () => {
    assertFalse(isValidUpdateChannel(''));
    assertFalse(isValidUpdateChannel('\uD800'));
    assertFalse(isValidUpdateChannel('\uDFFF'));
    assertFalse(isValidUpdateChannel('channel\uD800'));
    assertFalse(isValidUpdateChannel('\uDC00\uD800'));
  });
});
