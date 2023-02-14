// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DevicePageBrowserProxyImpl, Router, routes, SettingsPerDeviceKeyboardElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

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

  function initializePerDeviceKeyboardPage() {
    DevicePageBrowserProxyImpl.setInstanceForTesting(
        new TestDevicePageBrowserProxy());
    perDeviceKeyboardPage =
        document.createElement('settings-per-device-keyboard');
    assertTrue(perDeviceKeyboardPage != null);
    document.body.appendChild(perDeviceKeyboardPage);
    return flushTasks();
  }

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
