// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';

import {CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {ShortcutsElement} from 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {installMock} from './test_support.js';

suite('ShortcutsTest', () => {
  let customizeShortcuts: ShortcutsElement;
  let handler: TestBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(mock));
  });

  async function setInitialSettings(
      customLinksEnabled: boolean, shortcutsVisible: boolean): Promise<void> {
    handler.setResultFor('getMostVisitedSettings', Promise.resolve({
      customLinksEnabled,
      shortcutsVisible,
    }));
    customizeShortcuts = document.createElement('customize-chrome-shortcuts');
    document.body.appendChild(customizeShortcuts);
    await handler.whenCalled('getMostVisitedSettings');
  }

  test('shortcut element added to side panel', async () => {
    // We set initial settings in this test to demonstrate how to use the
    // handler. It does not affect the assertion in this test.
    setInitialSettings(true, true);
    assertTrue(document.body.contains(customizeShortcuts));
  });
});
