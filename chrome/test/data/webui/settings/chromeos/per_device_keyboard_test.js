// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DevicePageBrowserProxyImpl, fakeKeyboards, fakeKeyboards2, Router, routes, SettingsPerDeviceKeyboardElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('PerDeviceKeyboard', function() {
  /**
   * @type {?SettingsPerDeviceKeyboardElement}
   */
  let perDeviceKeyboardPage = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    perDeviceKeyboardPage = null;
    Router.getInstance().resetRouteForTesting();
  });

  function initializePerDeviceKeyboardPage(keyboards = fakeKeyboards) {
    DevicePageBrowserProxyImpl.setInstanceForTesting(
        new TestDevicePageBrowserProxy());
    perDeviceKeyboardPage =
        document.createElement('settings-per-device-keyboard');
    assertTrue(perDeviceKeyboardPage != null);
    perDeviceKeyboardPage.keyboards = keyboards;
    document.body.appendChild(perDeviceKeyboardPage);
    return flushTasks();
  }

  test('Keyboard page updates with new keyboards', async () => {
    await initializePerDeviceKeyboardPage();
    let subsections = perDeviceKeyboardPage.shadowRoot.querySelectorAll(
        'settings-per-device-keyboard-subsection');
    assertEquals(fakeKeyboards.length, subsections.length);

    // Check the number of subsections when the keyboard list is updated.
    perDeviceKeyboardPage.keyboards = fakeKeyboards2;
    await flushTasks();
    subsections = perDeviceKeyboardPage.shadowRoot.querySelectorAll(
        'settings-per-device-keyboard-subsection');
    assertEquals(fakeKeyboards2.length, subsections.length);
  });

  test(
      'Display correct name used for internal/external keyboards', async () => {
        await initializePerDeviceKeyboardPage();
        const subsections = perDeviceKeyboardPage.shadowRoot.querySelectorAll(
            'settings-per-device-keyboard-subsection');
        for (let i = 0; i < subsections.length; i++) {
          const name =
              subsections[i].shadowRoot.querySelector('h2').textContent;
          if (fakeKeyboards[i].isExternal) {
            assertEquals(fakeKeyboards[i].name, name);
          } else {
            assertTrue(subsections[i].i18nExists('builtInKeyboardName'));
            assertEquals('Built-in Keyboard', name);
          }
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

  test('Help message shown when no keyboards are connected', async () => {
    await initializePerDeviceKeyboardPage();
    perDeviceKeyboardPage.keyboards = [];
    await flushTasks();
    assertTrue(isVisible(perDeviceKeyboardPage.shadowRoot.querySelector(
        '#noKeyboardsConnectedContainer')));
    assertEquals(
        'No keyboard detected',
        perDeviceKeyboardPage.shadowRoot
            .querySelector('#noKeyboardsConnectedMessage')
            .innerText.trim());
  });
});
