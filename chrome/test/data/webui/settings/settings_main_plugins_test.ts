// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {Route, SettingsMainElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, Router, routes, setSearchManagerForTesting} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSearchManager} from './test_search_manager.js';

suite('SettingsMain', function() {
  let searchManager: TestSearchManager;
  let settingsMain: SettingsMainElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    searchManager = new TestSearchManager();
    setSearchManagerForTesting(searchManager);
    Router.getInstance().navigateTo(routes.BASIC);
    settingsMain = document.createElement('settings-main');
    settingsMain.prefs = settingsPrefs.prefs!;
    settingsMain.toolbarSpinnerActive = false;
    document.body.appendChild(settingsMain);
    flush();
  });

  test('UpdatesActiveViewWhenRouteChanges', async function() {
    function assertActive(pluginTag: string, path: string) {
      assertTrue(
          !!settingsMain.shadowRoot!.querySelector(
              `.active[slot=view] > ${pluginTag}`),
          `Element '${pluginTag}' was not active for route '${path}'`);
      assertFalse(!!settingsMain.shadowRoot!.querySelector(
          `.active[slot=view] > :not(${pluginTag}, dom-if)`));
    }

    // Check routes that are still residing within the old settings-basic-page
    // "plugin".
    const nonMigratedRoutes = [
      routes.BASIC,
      routes.AUTOFILL,
      routes.PRIVACY,
      routes.PERFORMANCE,
      routes.APPEARANCE,
      routes.SEARCH,
    ];

    for (const route of nonMigratedRoutes) {
      Router.getInstance().navigateTo(route);
      assertActive('settings-basic-page', route.path);
    }

    // Check routes that have been promoted to individual "plugins".
    const migratedRoutes: Array<{route: Route, pluginTag: string}> = [
      // TODO(crbug.com/424223101): Update this list as more routes are
      // migrated.

      // <if expr="not is_chromeos">
      {
        route: routes.DEFAULT_BROWSER,
        pluginTag: 'settings-default-browser-page',
      },
      // </if>
      {route: routes.ON_STARTUP, pluginTag: 'settings-on-startup-page'},
      {route: routes.LANGUAGES, pluginTag: 'settings-languages-page-index'},
      {route: routes.DOWNLOADS, pluginTag: 'settings-downloads-page'},
      {route: routes.ACCESSIBILITY, pluginTag: 'settings-a11y-page-index'},
      // <if expr="not is_chromeos">
      {route: routes.SYSTEM, pluginTag: 'settings-system-page'},
      // </if>
      {route: routes.RESET, pluginTag: 'settings-reset-page'},
      {route: routes.ABOUT, pluginTag: 'settings-about-page'},
    ];

    for (const {route, pluginTag} of migratedRoutes) {
      Router.getInstance().navigateTo(route);
      await flushTasks();
      assertActive(pluginTag, route.path);
    }
  });

  test('ShowAllViewsWhenSearching', async function() {
    // Check initial state.
    assertFalse(settingsMain.$.switcher.hasAttribute('show-all'));

    // Issue a search query. Expecting the cr-view-manager to show all views.
    await settingsMain.searchContents('foo');
    assertTrue(settingsMain.$.switcher.hasAttribute('show-all'));

    // Navigate to a subpage while search results are displayed. Expecting the
    // cr-view-manager to only show the active view.
    Router.getInstance().navigateTo(routes.FONTS);
    assertFalse(settingsMain.$.switcher.hasAttribute('show-all'));

    // Navigate back to the search results. Expecting the cr-view-manager to
    // show all views.
    Router.getInstance().navigateToPreviousRoute();
    await flushTasks();
    assertTrue(settingsMain.$.switcher.hasAttribute('show-all'));

    // Clear the search query. Expecting the cr-view-manager to only show the
    // active view.
    await settingsMain.searchContents('');
    assertFalse(settingsMain.$.switcher.hasAttribute('show-all'));
  });
});
