// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {installMock} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

suite('NewTabPageCustomizeShortcutsTest', () => {
  /** @type {!CustomizeShortcutsElement} */
  let customizeShortcuts;

  /** @type {!TestBrowserProxy} */
  let handler;

  setup(() => {
    PolymerTest.clearBody();

    handler = installMock(
        newTabPage.mojom.PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(
            mock, new newTabPage.mojom.PageCallbackRouter()));
  });

  /**
   * @param {boolean} customLinksEnabled
   * @param {boolean} visible
   * @return {!Promise}
   * @private
   */
  async function setInitialSettings(customLinksEnabled, shortcutsVisible) {
    handler.setResultFor('getMostVisitedSettings', Promise.resolve({
      customLinksEnabled,
      shortcutsVisible,
    }));
    customizeShortcuts = document.createElement('ntp-customize-shortcuts');
    document.body.appendChild(customizeShortcuts);
    await handler.whenCalled('getMostVisitedSettings');
  }

  /**
   * @param {boolean} selected
   * @param {!HTMLElement} el
   * @private
   */
  function assertIsSelected(selected, el) {
    assertEquals(selected, el.classList.contains('selected'));
  }

  /**
   * @param {boolean} customLinksEnabled
   * @param {boolean} useMostVisited
   * @param {boolean} hidden
   * @private
   */
  function assertSelection(customLinksEnabled, useMostVisited, hidden) {
    assertEquals(1, customLinksEnabled + useMostVisited + hidden);
    assertIsSelected(
        customLinksEnabled, customizeShortcuts.$.optionCustomLinks);
    assertIsSelected(useMostVisited, customizeShortcuts.$.optionMostVisited);
    assertIsSelected(hidden, customizeShortcuts.$.hide);
    assertEquals(hidden, customizeShortcuts.$.hideToggle.checked);
  }

  /** @private */
  function assertCustomLinksEnabled() {
    assertSelection(
        /* customLinksEnabled= */ true, /* useMostVisited= */ false,
        /* hidden= */ false);
  }

  /** @private */
  function assertUseMostVisited() {
    assertSelection(
        /* customLinksEnabled= */ false, /* useMostVisited= */ true,
        /* hidden= */ false);
  }

  /** @private */
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
