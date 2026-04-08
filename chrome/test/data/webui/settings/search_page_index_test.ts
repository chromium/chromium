// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import type {SettingsSearchPageIndexElement} from 'chrome://settings/settings.js';
import {loadTimeData, resetRouterForTesting, Router, routes, SearchEnginesBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';

suite('SearchPageIndex', function() {
  let index: SettingsSearchPageIndexElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      searchSettingsUpdate: false,
    });
    resetRouterForTesting();

    const browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);
    index = document.createElement('settings-search-page-index');
    index.prefs = {
      default_search_provider_data: {
        template_url_data: {},
      },
    };
    document.body.appendChild(index);
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
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

    Router.getInstance().navigateTo(routes.SEARCH_ENGINES);
    await microtasksFinished();
    assertActiveView('searchEngines');

    Router.getInstance().navigateTo(routes.SEARCH);
    await microtasksFinished();
    assertActiveView('parent');
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
  // inherited correctly.
  test('Search', async function() {
    // Test that the child view is properly annotated.
    assertTrue(!!index.$.viewManager.querySelector(
        '#searchEngines[slot=view][data-parent-view-id=parent]'));

    // Test that search finds results in both parent and child views.
    const result = await index.searchContents('Manage search engines');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
  });
});

suite('SearchPageIndexWithSearchSettingsUpdate', function() {
  let index: SettingsSearchPageIndexElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      searchSettingsUpdate: true,
    });
    resetRouterForTesting();

    index = document.createElement('settings-search-page-index');
    document.body.appendChild(index);
    return flushTasks();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  test('Routing', async function() {
    function assertActiveViews(ids: string[]) {
      for (const id of ids) {
        assertTrue(
            !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
      }
    }

    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveViews(
        ['parent', 'siteShortcuts', 'featureShortcuts', 'keyboardShortcut']);

    Router.getInstance().navigateTo(routes.SEARCH);
    await microtasksFinished();
    assertActiveViews(
        ['parent', 'siteShortcuts', 'featureShortcuts', 'keyboardShortcut']);
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
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

    // Results only in site shortcuts
    let result = await index.searchContents('sites and search engines');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(/*visible=*/['siteShortcuts'],
                       /*hidden=*/['parent']);

    // Results in site shortcuts, feature shortcuts and keyboard shortcut
    result = await index.searchContents('Keyboard key');
    assertFalse(result.canceled);
    assertEquals(3, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(
        /*visible=*/['siteShortcuts', 'featureShortcuts', 'keyboardShortcut'],
        /*hidden=*/['parent']);
    // Results only in feature shortcuts
    result = await index.searchContents('feature and extension shortcuts');
    assertFalse(result.canceled);
    assertEquals(2, result.matchCount);
    assertFalse(result.wasClearSearch);
    assertVisibleViews(/*visible=*/['featureShortcuts'], /*hidden=*/['parent']);
  });
});
