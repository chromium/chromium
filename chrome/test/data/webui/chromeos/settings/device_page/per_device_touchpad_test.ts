// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeTouchpads, fakeTouchpads2, PerDeviceSubsectionHeaderElement, SettingsPerDeviceTouchpadElement} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-per-device-touchpad>', () => {
  let perDeviceTouchpadPage: SettingsPerDeviceTouchpadElement;

  setup(async () => {
    perDeviceTouchpadPage =
        document.createElement('settings-per-device-touchpad');
    perDeviceTouchpadPage.set('touchpads', fakeTouchpads);
    document.body.appendChild(perDeviceTouchpadPage);
    await flushTasks();
  });

  teardown(() => {
    perDeviceTouchpadPage.remove();
  });

  test('Touchpad page updates with new touchpads', async () => {
    let subsections = perDeviceTouchpadPage.shadowRoot!.querySelectorAll(
        'settings-per-device-touchpad-subsection');
    assertEquals(fakeTouchpads.length, subsections.length);
    assertFalse(subsections[0]!.get('isLastDevice'));
    assertTrue(subsections[fakeTouchpads.length - 1]!.get('isLastDevice'));

    // Check the number of subsections when the touchpad list is updated.
    perDeviceTouchpadPage.set('touchpads', fakeTouchpads2);
    await flushTasks();
    subsections = perDeviceTouchpadPage.shadowRoot!.querySelectorAll(
        'settings-per-device-touchpad-subsection');
    assertEquals(fakeTouchpads2.length, subsections.length);
    assertTrue(subsections[fakeTouchpads2.length - 1]!.get('isLastDevice'));
  });

  test(
      'Display correct name used for internal/external touchpads', async () => {
        const subsections = perDeviceTouchpadPage.shadowRoot!.querySelectorAll(
            'settings-per-device-touchpad-subsection');
        for (let i = 0; i < subsections.length; i++) {
          const subsectionHeader = strictQuery(
              'per-device-subsection-header', subsections[i]!.shadowRoot,
              PerDeviceSubsectionHeaderElement);
          const name =
              subsectionHeader.shadowRoot!.querySelector(
                                              'h2')!.textContent!.trim();
          if (fakeTouchpads[i]!.isExternal) {
            assertEquals(fakeTouchpads[i]!.name, name);
          } else {
            assertTrue(subsections[i]!.i18nExists('builtInTouchpadName'));
            assertEquals('Built-in Touchpad', name);
          }
        }
      });
});
