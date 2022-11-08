// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceFeatureState, SyncBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSyncBrowserProxy} from './test_os_sync_browser_proxy.js';

suite('Multidevice', function() {
  let combinedSetupItem;

  setup(function() {
    const browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    PolymerTest.clearBody();

    combinedSetupItem =
        document.createElement('settings-multidevice-combined-setup-item');
    document.body.appendChild(combinedSetupItem);

    flush();
  });

  teardown(function() {
    combinedSetupItem.remove();
  });

  test('Setup button is disabled when PhoneHub is disabled.', async () => {
    let button = combinedSetupItem.shadowRoot.querySelector(
        'cr-button[slot=feature-controller]');
    assertTrue(!!button);
    assertFalse(button.disabled);

    combinedSetupItem.pageContentData = Object.assign(
        {}, combinedSetupItem.pageContentData,
        {phoneHubState: MultiDeviceFeatureState.DISABLED_BY_USER});
    flush();

    button = combinedSetupItem.shadowRoot.querySelector(
        'cr-button[slot=feature-controller]');
    assertTrue(!!button);
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
            combinedSetupItem.getSetupName_(),
            combinedSetupItem.i18n(
                'multidevicePhoneHubCameraRollNotificationsAndAppsItemTitle'));
        assertEquals(
            combinedSetupItem.getSetupSummary_(),
            combinedSetupItem.i18n(
                'multidevicePhoneHubCameraRollNotificationsAndAppsItemSummary'));
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
            combinedSetupItem.getSetupName_(),
            combinedSetupItem.i18n(
                'multidevicePhoneHubCameraRollAndNotificationsItemTitle'));
        assertEquals(
            combinedSetupItem.getSetupSummary_(),
            combinedSetupItem.i18n(
                'multidevicePhoneHubCameraRollAndNotificationsItemSummary'));
      });

  test('Correct strings are shown for camera roll and apps.', async () => {
    combinedSetupItem.setProperties({
      cameraRoll: true,
      appStreaming: true,
    });
    flush();

    assertEquals(
        combinedSetupItem.getSetupName_(),
        combinedSetupItem.i18n(
            'multidevicePhoneHubCameraRollAndAppsItemTitle'));
    assertEquals(
        combinedSetupItem.getSetupSummary_(),
        combinedSetupItem.i18n(
            'multidevicePhoneHubCameraRollAndAppsItemSummary'));
  });

  test('Correct strings are shown for notifications and apps.', async () => {
    combinedSetupItem.setProperties({
      notifications: true,
      appStreaming: true,
    });
    flush();

    assertEquals(
        combinedSetupItem.getSetupName_(),
        combinedSetupItem.i18n(
            'multidevicePhoneHubAppsAndNotificationsItemTitle'));
    assertEquals(
        combinedSetupItem.getSetupSummary_(),
        combinedSetupItem.i18n(
            'multidevicePhoneHubAppsAndNotificationsItemSummary'));
  });
});
