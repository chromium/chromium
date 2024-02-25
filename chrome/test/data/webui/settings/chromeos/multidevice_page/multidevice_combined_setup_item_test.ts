// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SettingsMultideviceCombinedSetupItemElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, MultiDeviceFeatureState, SyncBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSyncBrowserProxy} from '../test_os_sync_browser_proxy.js';

suite('<settings-multidevice-combined-setup-item>', () => {
  let combinedSetupItem: SettingsMultideviceCombinedSetupItemElement;

  setup(() => {
    const browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    combinedSetupItem =
        document.createElement('settings-multidevice-combined-setup-item');
    document.body.appendChild(combinedSetupItem);

    flush();
  });

  teardown(() => {
    combinedSetupItem.remove();
  });

  test('Setup button is disabled when PhoneHub is disabled.', async () => {
    let button = combinedSetupItem.shadowRoot!.querySelector<CrButtonElement>(
        'cr-button[slot=feature-controller]');
    assert(button);
    assertFalse(button.disabled);

    combinedSetupItem.pageContentData = Object.assign(
        {}, combinedSetupItem.pageContentData,
        {phoneHubState: MultiDeviceFeatureState.DISABLED_BY_USER});
    flush();

    button = combinedSetupItem.shadowRoot!.querySelector(
        'cr-button[slot=feature-controller]');
    assert(button);
    assertTrue(button.disabled);
  });

  test(
      'Correct strings are shown for camera roll, notifications and apps.',
      async () => {
        combinedSetupItem.setProperties({
          cameraRoll: true,
          notifications: true,
          appStreaming: true,
        });
        flush();

        assertEquals(
            combinedSetupItem.i18n(
                'multidevicePhoneHubCameraRollNotificationsAndAppsItemTitle'),
            combinedSetupItem.get('setupName_'));
        assertEquals(
            combinedSetupItem.i18n(
                'multidevicePhoneHubCameraRollNotificationsAndAppsItemSummary'),
            combinedSetupItem.get('setupSummary_'));
      });

  test(
      'Correct strings are shown for camera roll and notifications.',
      async () => {
        combinedSetupItem.setProperties({
          cameraRoll: true,
          notifications: true,
        });
        flush();

        assertEquals(
            combinedSetupItem.i18n(
                'multidevicePhoneHubCameraRollAndNotificationsItemTitle'),
            combinedSetupItem.get('setupName_'));
        assertEquals(
            combinedSetupItem.i18n(
                'multidevicePhoneHubCameraRollAndNotificationsItemSummary'),
            combinedSetupItem.get('setupSummary_'));
      });

  test('Correct strings are shown for camera roll and apps.', async () => {
    combinedSetupItem.setProperties({
      cameraRoll: true,
      appStreaming: true,
    });
    flush();

    assertEquals(
        combinedSetupItem.i18n('multidevicePhoneHubCameraRollAndAppsItemTitle'),
        combinedSetupItem.get('setupName_'));
    assertEquals(
        combinedSetupItem.i18n(
            'multidevicePhoneHubCameraRollAndAppsItemSummary'),
        combinedSetupItem.get('setupSummary_'));
  });

  test('Correct strings are shown for notifications and apps.', async () => {
    combinedSetupItem.setProperties({
      notifications: true,
      appStreaming: true,
    });
    flush();

    assertEquals(
        combinedSetupItem.i18n(
            'multidevicePhoneHubAppsAndNotificationsItemTitle'),
        combinedSetupItem.get('setupName_'));
    assertEquals(
        combinedSetupItem.i18n(
            'multidevicePhoneHubAppsAndNotificationsItemSummary'),
        combinedSetupItem.get('setupSummary_'));
  });
});
