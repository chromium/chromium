// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/lazy_load.js';

import {CustomizeShortcutsElement} from 'chrome://new-tab-page/lazy_load.js';
import {NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {installMock} from './test_support.js';

suite('NewTabPageCustomizeShortcutsTest', () => {
  let customizeShortcuts: CustomizeShortcutsElement;
  let handler: TestBrowserProxy<PageHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
  });

  async function setInitialSettings(
      customLinksEnabled: boolean, shortcutsVisible: boolean): Promise<void> {
    handler.setResultFor('getMostVisitedSettings', Promise.resolve({
      customLinksEnabled,
      shortcutsVisible,
    }));
    customizeShortcuts = document.createElement('ntp-customize-shortcuts');
    document.body.appendChild(customizeShortcuts);
    await handler.whenCalled('getMostVisitedSettings');
  }

  function assertIsSelected(selected: boolean, el: HTMLElement) {
    assertEquals(selected, el.classList.contains('selected'));
  }

  function assertSelection(
      customLinksEnabled: boolean, useMostVisited: boolean, hidden: boolean) {
    assertEquals(
        1,
        Number(customLinksEnabled) + Number(useMostVisited) + Number(hidden));
    assertIsSelected(
        customLinksEnabled, customizeShortcuts.$.optionCustomLinks);
    assertIsSelected(useMostVisited, customizeShortcuts.$.optionMostVisited);
    assertIsSelected(hidden, customizeShortcuts.$.hide);
    assertEquals(hidden, customizeShortcuts.$.hideToggle.checked);
  }

  function assertCustomLinksEnabled() {
    assertSelection(
        /* customLinksEnabled= */ true, /* useMostVisited= */ false,
        /* hidden= */ false);
  }

  function assertUseMostVisited() {
    assertSelection(
        /* customLinksEnabled= */ false, /* useMostVisited= */ true,
        /* hidden= */ false);
  }

  function assertHidden() {
    assertSelection(
        /* customLinksEnabled= */ false, /* useMostVisited= */ false,
        /* hidden= */ true);
  }

  test('selections are mutually exclusive', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ false);
    assertIsSelected(false, customizeShortcuts.$.optionCustomLinks);
    customizeShortcuts.$.optionCustomLinksButton.click();
    assertCustomLinksEnabled();
    customizeShortcuts.$.optionMostVisitedButton.click();
    assertUseMostVisited();
    customizeShortcuts.$.hideToggle.click();
    assertHidden();
    customizeShortcuts.$.hideToggle.click();
    assertUseMostVisited();
  });

  test('enable custom links calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ false, /* shortcutsVisible= */ false);
    assertHidden();
    customizeShortcuts.$.optionCustomLinksButton.click();
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    customizeShortcuts.apply();
    const [customLinksEnabled, visible] = await setSettingsCalled;
    assertTrue(customLinksEnabled);
    assertTrue(visible);
  });

  test('use most-visited calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ false);
    assertHidden();
    customizeShortcuts.$.optionMostVisitedButton.click();
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    customizeShortcuts.apply();
    const [customLinksEnabled, visible] = await setSettingsCalled;
    assertFalse(customLinksEnabled);
    assertTrue(visible);
  });

  test('toggle hide calls setMostVisitedSettings', async () => {
    await setInitialSettings(
        /* customLinksEnabled= */ true, /* shortcutsVisible= */ true);
    assertCustomLinksEnabled();
    customizeShortcuts.$.hideToggle.click();
    const setSettingsCalled = handler.whenCalled('setMostVisitedSettings');
    customizeShortcuts.apply();
    const [customLinksEnabled, visible] = await setSettingsCalled;
    assertTrue(customLinksEnabled);
    assertFalse(visible);
  });
});
