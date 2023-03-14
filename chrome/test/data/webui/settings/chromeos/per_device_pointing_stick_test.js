// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeInputDeviceSettingsProvider, fakePointingSticks, setInputDeviceSettingsProviderForTesting, SettingsPerDevicePointingStickElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('PerDevicePointingStick', function() {
  /**
   * @type {?SettingsPerDevicePointingStickElement}
   */
  let perDevicePointingStickPage = null;
  /**
   * @type {?FakeInputDeviceSettingsProvider}
   */
  let provider = null;

  setup(() => {
    PolymerTest.clearBody();
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakePointingSticks(fakePointingSticks);
    setInputDeviceSettingsProviderForTesting(provider);
  });

  teardown(() => {
    perDevicePointingStickPage = null;
    provider = null;
  });

  function initializePerDevicePointingStickPage() {
    perDevicePointingStickPage =
        document.createElement('settings-per-device-pointing-stick');
    assertTrue(perDevicePointingStickPage != null);
    document.body.appendChild(perDevicePointingStickPage);
    return flushTasks();
  }

  test('PointingStick page updates with new pointing sticks', async () => {
    await initializePerDevicePointingStickPage();
    let subsections = perDevicePointingStickPage.shadowRoot.querySelectorAll(
        'settings-per-device-pointing-stick-subsection');
    assertEquals(fakePointingSticks.length, subsections.length);
    for (let i = 0; i < subsections.length; i++) {
      const name = subsections[i].shadowRoot.querySelector('h2').textContent;
      assertEquals(fakePointingSticks[i].name, name);
    }

    const newFakePointingSticks = fakePointingSticks.slice(1);
    provider.setFakePointingSticks(newFakePointingSticks);
    await flushTasks();
    subsections = perDevicePointingStickPage.shadowRoot.querySelectorAll(
        'settings-per-device-pointing-stick-subsection');
    assertEquals(newFakePointingSticks.length, subsections.length);
    for (let i = 0; i < subsections.length; i++) {
      const name = subsections[i].shadowRoot.querySelector('h2').textContent;
      assertEquals(newFakePointingSticks[i].name, name);
    }
  });
});