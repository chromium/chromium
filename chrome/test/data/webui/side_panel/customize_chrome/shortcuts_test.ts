// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {ShortcutsElement} from 'chrome://customize-chrome-side-panel.top-chrome/shortcuts.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from './test_support.js';

suite('ShortcutsTest', () => {
  let customizeShortcutsElement: ShortcutsElement;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouterRemote: CustomizeChromePageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function setInitialSettings(
      customLinksEnabled: boolean, shortcutsVisible: boolean): Promise<void> {
    customizeShortcutsElement =
        document.createElement('customize-chrome-shortcuts');
    document.body.appendChild(customizeShortcutsElement);
    await handler.whenCalled('updateMostVisitedSettings');
    callbackRouterRemote.setMostVisitedSettings(
        customLinksEnabled, shortcutsVisible);
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(customizeShortcutsElement.$.customLinksButton.hideLabelText);
    assertTrue(customizeShortcutsElement.$.mostVisitedButton.hideLabelText);
    assertEquals(
        customizeShortcutsElement.$.customLinksButton.label, 'My shortcuts');
    assertEquals(
        customizeShortcutsElement.$.mostVisitedButton.label,
        'Most visited sites');
  }

  function assertShown(shown: boolean) {
    assertEquals(
        shown, customizeShortcutsElement.$.showShortcutsToggle.checked);
  }

  function assertCustomLinksEnabled() {
    assertEquals(
        'customLinksOption',
        customizeShortcutsElement.$.shortcutsRadioSelection.selected);
    assertShown(true);
  }

  function assertUseMostVisited() {
    assertEquals(
        'mostVisitedOption',
        customizeShortcutsElement.$.shortcutsRadioSelection.selected);
    assertShown(true);
  }

  test('selections are mutually exclusive', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ false);
    assertShown(false);
    customizeShortcutsElement.$.showShortcutsToggle.click();
    assertCustomLinksEnabled();
    customizeShortcutsElement.$.mostVisitedButton.click();
    assertUseMostVisited();
    customizeShortcutsElement.$.showShortcutsToggle.click();
    assertShown(false);
    customizeShortcutsElement.$.showShortcutsToggle.click();
    assertUseMostVisited();
  });

  test('toggling show shortcuts on calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ false);
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    customizeShortcutsElement.$.showShortcutsToggle.click();
    const [customLinksEnabled, shortcutsVisible] = await setSettingsCalled;
    const selector =
        customizeShortcutsElement.shadowRoot!.querySelector('iron-collapse');

    assertTrue(!!selector);
    assertEquals(true, selector.opened);
    assertFalse(customLinksEnabled);
    assertTrue(shortcutsVisible);
  });

  test('turning toggle off hides settings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ true);
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    customizeShortcutsElement.$.showShortcutsToggle.click();
    const [customLinksEnabled, shortcutsVisible] = await setSettingsCalled;
    const selector =
        customizeShortcutsElement.shadowRoot!.querySelector('iron-collapse');

    assertTrue(!!selector);
    assertEquals(false, selector.opened);
    assertFalse(customLinksEnabled);
    assertFalse(shortcutsVisible);
  });

  test('enable custom links calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ true);
    assertUseMostVisited();
    customizeShortcutsElement.$.customLinksButton.click();
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    const [customLinksEnabled, shortcutsVisible] = await setSettingsCalled;
    assertTrue(customLinksEnabled);
    assertTrue(shortcutsVisible);
  });

  test('enable most visited calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ true);
    assertCustomLinksEnabled();
    customizeShortcutsElement.$.mostVisitedButton.click();
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    const [customLinksEnabled, shortcutsVisible] = await setSettingsCalled;
    assertFalse(customLinksEnabled);
    assertTrue(shortcutsVisible);
  });

  test('only animates after initialization', async () => {
    customizeShortcutsElement =
        document.createElement('customize-chrome-shortcuts');
    document.body.appendChild(customizeShortcutsElement);
    const ironCollapse =
        customizeShortcutsElement.shadowRoot!.querySelector('iron-collapse')!;

    // No animation before initialize.
    assertTrue(ironCollapse.noAnimation!);

    // Initialize.
    callbackRouterRemote.setMostVisitedSettings(
        /*customLinksEnabled=*/ true, /*shortcutsVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Animation after initialize.
    assertFalse(ironCollapse.noAnimation!);

    // Update.
    callbackRouterRemote.setMostVisitedSettings(
        /*customLinksEnabled=*/ false, /*shortcutsVisible=*/ true);
    await callbackRouterRemote.$.flushForTesting();

    // Still animation after update.
    assertFalse(ironCollapse.noAnimation!);
  });
});
