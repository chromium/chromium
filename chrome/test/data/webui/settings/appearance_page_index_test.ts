// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import type {SettingsAppearancePageIndexElement} from 'chrome://settings/settings.js';
import {AppearanceBrowserProxyImpl, CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestAppearanceBrowserProxy} from './test_appearance_browser_proxy.js';

suite('AppearancePageIndex', function() {
  let index: SettingsAppearancePageIndexElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const browserProxy = new TestAppearanceBrowserProxy();
    AppearanceBrowserProxyImpl.setInstance(browserProxy);

    const settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;
    index = document.createElement('settings-appearance-page-index');
    index.prefs = settingsPrefs.prefs!;
    document.body.appendChild(index);
    return flushTasks();
  });

  test('Routing', async function() {
    function assertActiveView(id: string) {
      assertTrue(
          !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
      assertFalse(!!index.$.viewManager.querySelector(
          `.active[slot=view]:not(#${id})`));
    }

    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveView('parent');

    Router.getInstance().navigateTo(routes.FONTS);
    await microtasksFinished();
    assertActiveView('fonts');

    Router.getInstance().navigateTo(routes.APPEARANCE);
    await microtasksFinished();
    assertActiveView('parent');
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
  // inherited correctly.
  test('Search', async function() {
    // Test that the child view is properly annotated.
    assertTrue(!!index.$.viewManager.querySelector(
        '#fonts[slot=view][data-parent-view-id=parent]'));

    // Test that search finds results in both parent and child views.
    const result = await index.searchContents('Customize fonts');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
  });
});
