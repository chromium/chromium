// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsGeneralPageIndexElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefService, resetPageVisibilityForTesting, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('GeneralPageIndex', function() {
  let index: SettingsGeneralPageIndexElement;

  const defaultViews = [
    'onStartup',
    'downloads',
    // <if expr="not is_chromeos">
    'system',
    // </if>
  ];

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      settingsRefresh2026: 'settings-refresh-2026',
    });
    resetPageVisibilityForTesting();
    resetRouterForTesting();

    await PrefService.getInstance().whenInitialized();
    index = document.createElement('settings-general-page-index');
    document.body.appendChild(index);
    return microtasksFinished();
  });

  test('Routing', async function() {
    function assertActiveViews(ids: string[]) {
      for (const id of ids) {
        assertTrue(
            !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`),
            id);
      }
    }

    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveViews(defaultViews);

    Router.getInstance().navigateTo(routes.GENERAL);
    await microtasksFinished();
    assertActiveViews(defaultViews);
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixinLit is
  // inherited correctly.
  test('Search', async function() {
    function assertVisibleViews(visible: string[], hidden: string[]) {
      for (const id of visible) {
        assertTrue(isVisible(index.$.viewManager.querySelector(`#${id}`)), id);
      }

      for (const id of hidden) {
        assertFalse(isVisible(index.$.viewManager.querySelector(`#${id}`)), id);
      }
    }

    // Case 1: Results only in settings-on-startup-page.
    let result = await index.searchContents('Continue where you left off');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(
        ['onStartup'], defaultViews.filter(id => id !== 'onStartup'));

    // Case 2: Results only in settings-downloads-page.
    result = await index.searchContents('Ask where to save each file');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(
        ['downloads'], defaultViews.filter(id => id !== 'downloads'));

    // <if expr="not is_chromeos">
    // Case 3: Results only in settings-system-page.
    result = await index.searchContents('Open your computer\'s proxy settings');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(['system'], defaultViews.filter(id => id !== 'system'));
    // </if>

    // Case 4: No results hides all views.
    result = await index.searchContents('non-existing-text');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews([], defaultViews);

    // Case 5: Clearing search restores all views.
    result = await index.searchContents('');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertTrue(result.wasClearSearch);
    assertVisibleViews(defaultViews, []);
  });
});
