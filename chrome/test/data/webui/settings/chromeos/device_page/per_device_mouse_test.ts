// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeInputDeviceSettingsProvider, fakeMice, setInputDeviceSettingsProviderForTesting, SettingsPerDeviceMouseElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-per-device-mouse>', function() {
  let perDeviceMousePage: SettingsPerDeviceMouseElement;
  let provider: FakeInputDeviceSettingsProvider;

  setup(() => {
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeMice(fakeMice);
    setInputDeviceSettingsProviderForTesting(provider);
  });

  teardown(() => {
    perDeviceMousePage.remove();
  });

  function initializePerDeviceMousePage() {
    perDeviceMousePage = document.createElement('settings-per-device-mouse');
    assertTrue(!!perDeviceMousePage);
    document.body.appendChild(perDeviceMousePage);
    return flushTasks();
  }

  test('Mouse page updates with new mice', async () => {
    await initializePerDeviceMousePage();
    let subsections = perDeviceMousePage.shadowRoot!.querySelectorAll(
        'settings-per-device-mouse-subsection');
    assertEquals(fakeMice.length, subsections.length);
    for (let i = 0; i < subsections.length; i++) {
      const name = subsections[i]!.shadowRoot!.querySelector('h2')!.textContent;
      assertEquals(fakeMice[i]!.name, name);
    }

    const newFakeMice = fakeMice.slice(1);
    provider.setFakeMice(newFakeMice);
    await flushTasks();
    subsections = perDeviceMousePage.shadowRoot!.querySelectorAll(
        'settings-per-device-mouse-subsection');
    assertEquals(newFakeMice.length, subsections.length);
    for (let i = 0; i < subsections.length; i++) {
      const name = subsections[i]!.shadowRoot!.querySelector('h2')!.textContent;
      assertEquals(newFakeMice[i]!.name, name);
    }
  });
});
