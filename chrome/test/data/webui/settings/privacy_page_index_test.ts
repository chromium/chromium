// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import type {Route, SettingsPrivacyPageIndexElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, resetPageVisibilityForTesting, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('PrivacyPageIndex', function() {
  let index: SettingsPrivacyPageIndexElement;

  async function createPrivacyPageIndex(overrides?: {[key: string]: any}) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues(Object.assign(
        {
          isGuest: false,
        },
        overrides || {}));
    resetPageVisibilityForTesting();
    resetRouterForTesting();

    const settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    index = document.createElement('settings-privacy-page-index');
    index.prefs = settingsPrefs.prefs!;
    Router.getInstance().navigateTo(routes.BASIC);
    document.body.appendChild(index);
    return flushTasks();
  }

  setup(function() {
    return createPrivacyPageIndex();
  });

  test('Routing', async function() {
    function assertActiveViews(ids: string[]) {
      for (const id of ids) {
        assertTrue(
            !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
      }
    }

    const defaultViews = ['old', 'privacyGuidePromo', 'safetyHubEntryPoint'];

    Router.getInstance().navigateTo(routes.PRIVACY);
    await flushTasks();
    await waitBeforeNextRender(index);
    assertActiveViews(defaultViews);

    Router.getInstance().navigateTo(routes.BASIC);
    await flushTasks();
    await waitBeforeNextRender(index);
    assertActiveViews(defaultViews);

    // Non-exhaustive list of PRIVACY child routes that have not been migrated
    // to the new architecture (crbug.com/424223101), therefore the contents
    // still reside in the 'old' <settings-basic-page> view.
    const nonMigratedRoutes: Route[] = [
      routes.CLEAR_BROWSER_DATA,
      routes.COOKIES,
      routes.SAFETY_HUB,
      routes.SECURITY,
      routes.SITE_SETTINGS,
      routes.SITE_SETTINGS_LOCATION,
    ];

    for (const route of nonMigratedRoutes) {
      Router.getInstance().navigateTo(route);
      await flushTasks();
      await waitBeforeNextRender(index);
      assertActiveViews(['old']);
    }
  });

  // TODO(crbug.com/424223101): Remove this test once <settings-basic-page> is
  // removed.
  test('RoutingLazyRender', async function() {
    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    await flushTasks();
    await waitBeforeNextRender(index);
    assertFalse(!!index.$.viewManager.querySelector('#old'));

    Router.getInstance().navigateTo(routes.PRIVACY);
    await flushTasks();
    await waitBeforeNextRender(index);
    assertTrue(!!index.$.viewManager.querySelector('#old.active'));
  });

  // <if expr="is_chromeos">
  test('RoutingGuestMode', async function() {
    assertFalse(loadTimeData.getBoolean('isGuest'));
    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    await createPrivacyPageIndex({isGuest: true});
    assertTrue(!!index.$.viewManager.querySelector('#old.active[slot=view]'));
  });
  // </if>

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
  // inherited correctly.
  test('Search', async function() {
    index.inSearchMode = true;
    await flushTasks();

    // Case1: Results within the "Privacy and security" card.
    let result = await index.searchContents('Privacy and security');
    assertFalse(result.canceled);
    assertTrue(result.matchCount > 0);
    assertFalse(result.wasClearSearch);

    // Case2: Results within the "Safety check" card.
    result = await index.searchContents('Safety check');
    assertFalse(result.canceled);
    assertTrue(result.matchCount > 0);
    assertFalse(result.wasClearSearch);
  });
});
