// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {ShortcutsElement} from 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {installMock} from './test_support.js';

suite('ShortcutsTest', () => {
  let customizeShortcuts: ShortcutsElement;
  let handler: TestBrowserProxy<CustomizeChromePageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
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

  function assertShown(shown: boolean) {
    assertEquals(shown, customizeShortcuts.$.showShortcutsToggle.checked);
  }

  function assertCustomLinksEnabled() {
    assertEquals(
        'customLinksOption',
        customizeShortcuts.$.shortcutsRadioSelection.selected);
    assertShown(true);
  }

  function assertUseMostVisited() {
    assertEquals(
        'mostVisitedOption',
        customizeShortcuts.$.shortcutsRadioSelection.selected);
    assertShown(true);
  }

  test('selections are mutually exclusive', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ false);
    assertShown(false);
    customizeShortcuts.$.showShortcutsToggle.click();
    assertCustomLinksEnabled();
    customizeShortcuts.$.mostVisitedButton.click();
    assertUseMostVisited();
    customizeShortcuts.$.showShortcutsToggle.click();
    assertShown(false);
    customizeShortcuts.$.showShortcutsToggle.click();
    assertUseMostVisited();
  });

  test('toggling show shortcuts on calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ false);
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    customizeShortcuts.$.showShortcutsToggle.click();
    const [customLinksEnabled, visible] = await setSettingsCalled;
    assertFalse(customLinksEnabled);
    assertTrue(visible);
  });

  test('enable custom links calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ true);
    assertUseMostVisited();
    customizeShortcuts.$.customLinksButton.click();
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    const [customLinksEnabled, visible] = await setSettingsCalled;
    assertTrue(customLinksEnabled);
    assertTrue(visible);
  });

  test('enable most visited calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ true);
    assertCustomLinksEnabled();
    customizeShortcuts.$.mostVisitedButton.click();
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    const [customLinksEnabled, visible] = await setSettingsCalled;
    assertFalse(customLinksEnabled);
    assertTrue(visible);
  });
});
