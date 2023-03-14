// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DevicePageBrowserProxyImpl, FakeInputDeviceSettingsProvider, fakeKeyboards, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsPerDeviceKeyboardElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('PerDeviceKeyboard', function() {
  /**
   * @type {?SettingsPerDeviceKeyboardElement}
   */
  let perDeviceKeyboardPage = null;
  /**
   * @type {?FakeInputDeviceSettingsProvider}
   */
  let provider = null;

  setup(() => {
    PolymerTest.clearBody();
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeKeyboards(fakeKeyboards);
    setInputDeviceSettingsProviderForTesting(provider);
  });

  teardown(() => {
    perDeviceKeyboardPage = null;
    provider = null;
    Router.getInstance().resetRouteForTesting();
  });

  function initializePerDeviceKeyboardPage() {
    DevicePageBrowserProxyImpl.setInstanceForTesting(
        new TestDevicePageBrowserProxy());
    perDeviceKeyboardPage =
        document.createElement('settings-per-device-keyboard');
    assertTrue(perDeviceKeyboardPage != null);
    document.body.appendChild(perDeviceKeyboardPage);
    return flushTasks();
  }

  test('Keyboard page updates with new keyboards', async () => {
    await initializePerDeviceKeyboardPage();
    let subsections = perDeviceKeyboardPage.shadowRoot.querySelectorAll(
        'settings-per-device-keyboard-subsection');
    assertEquals(fakeKeyboards.length, subsections.length);
    for (let i = 0; i < subsections.length; i++) {
      const name = subsections[i].shadowRoot.querySelector('h2').textContent;
      assertEquals(fakeKeyboards[i].name, name);
    }

    const newFakeKeyboards = fakeKeyboards.slice(1);
    provider.setFakeKeyboards(newFakeKeyboards);
    await flushTasks();
    subsections = perDeviceKeyboardPage.shadowRoot.querySelectorAll(
        'settings-per-device-keyboard-subsection');
    assertEquals(newFakeKeyboards.length, subsections.length);
    for (let i = 0; i < subsections.length; i++) {
      const name = subsections[i].shadowRoot.querySelector('h2').textContent;
      assertEquals(newFakeKeyboards[i].name, name);
    }
  });

  test('Open keyboard shortcut viewer', async () => {
    await initializePerDeviceKeyboardPage();
    perDeviceKeyboardPage.shadowRoot.querySelector('#keyboardShortcutViewer')
        .click();
    assertEquals(
        1,
        DevicePageBrowserProxyImpl.getInstance().keyboardShortcutViewerShown_);
  });

  test('Navigate to input tab', async () => {
    await initializePerDeviceKeyboardPage();
    perDeviceKeyboardPage.shadowRoot.querySelector('#showLanguagesInput')
        .click();
    assertEquals(routes.OS_LANGUAGES_INPUT, Router.getInstance().currentRoute);
  });
});
