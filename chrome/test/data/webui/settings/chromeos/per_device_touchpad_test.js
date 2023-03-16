// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeInputDeviceSettingsProvider, fakeTouchpads, setInputDeviceSettingsProviderForTesting, SettingsPerDeviceTouchpadElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('PerDeviceTouchpad', function() {
  /**
   * @type {?SettingsPerDeviceTouchpadElement}
   */
  let perDeviceTouchpadPage = null;
  /**
   * @type {?FakeInputDeviceSettingsProvider}
   */
  let provider = null;

  setup(() => {
    PolymerTest.clearBody();
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeTouchpads(fakeTouchpads);
    setInputDeviceSettingsProviderForTesting(provider);
  });

  teardown(() => {
    perDeviceTouchpadPage = null;
    provider = null;
  });

  function initializePerDeviceTouchpadPage() {
    perDeviceTouchpadPage =
        document.createElement('settings-per-device-touchpad');
    assertTrue(perDeviceTouchpadPage != null);
    document.body.appendChild(perDeviceTouchpadPage);
    return flushTasks();
  }

  test('Touchpad page updates with new touchpads', async () => {
    await initializePerDeviceTouchpadPage();
    let subsections = perDeviceTouchpadPage.shadowRoot.querySelectorAll(
        'settings-per-device-touchpad-subsection');
    assertEquals(fakeTouchpads.length, subsections.length);

    const newFakeTouchpads = fakeTouchpads.slice(1);
    provider.setFakeTouchpads(newFakeTouchpads);
    await flushTasks();
    subsections = perDeviceTouchpadPage.shadowRoot.querySelectorAll(
        'settings-per-device-touchpad-subsection');
    assertEquals(newFakeTouchpads.length, subsections.length);
  });

  test(
      'Display correct name used for internal/external touchpads', async () => {
        await initializePerDeviceTouchpadPage();
        const subsections = perDeviceTouchpadPage.shadowRoot.querySelectorAll(
            'settings-per-device-touchpad-subsection');
        for (let i = 0; i < subsections.length; i++) {
          const name =
              subsections[i].shadowRoot.querySelector('h2').textContent;
          if (fakeTouchpads[i].isExternal) {
            assertEquals(fakeTouchpads[i].name, name);
          } else {
            assertTrue(subsections[i].i18nExists('builtInTouchpadName'));
            assertEquals('Built-in Touchpad', name);
          }
        }
      });
});
