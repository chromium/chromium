// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsHomePageIndexElement} from 'chrome://settings/settings.js';
import {loadTimeData, resetPageVisibilityForTesting, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('HomePageIndex', function() {
  let index: SettingsHomePageIndexElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      settingsRefresh2026: 'settings-refresh-2026',
    });
    resetPageVisibilityForTesting();
    resetRouterForTesting();

    index = document.createElement('settings-home-page-index');
    document.body.appendChild(index);
    return microtasksFinished();
  });

  test('Routing', function() {
    function assertActiveView(id: string) {
      assertTrue(
          !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
      assertFalse(!!index.$.viewManager.querySelector(
          `.active[slot=view]:not(#${id})`));
    }

    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveView('parent');
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixinLit is
  // inherited correctly.
  test('Search', async function() {
    let result = await index.searchContents('non-existing-text');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertTrue(
        !!index.$.viewManager.querySelector('#parent[hidden-by-search]'));

    result = await index.searchContents('');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertTrue(result.wasClearSearch);
    assertTrue(
        !!index.$.viewManager.querySelector('#parent:not([hidden-by-search])'));
  });
});
