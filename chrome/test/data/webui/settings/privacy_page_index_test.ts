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
          enableSecurityKeysSubpage: false,
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

  function assertActiveViews(ids: string[]) {
    for (const id of ids) {
      assertTrue(
          !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
    }
  }

  setup(function() {
    return createPrivacyPageIndex();
  });

  test('Routing', async function() {
    const defaultViews = ['old', 'privacyGuidePromo', 'safetyHubEntryPoint'];

    Router.getInstance().navigateTo(routes.PRIVACY);
    await flushTasks();
    await waitBeforeNextRender(index);
    assertActiveViews(defaultViews);

    Router.getInstance().navigateTo(routes.BASIC);
    await flushTasks();
    await waitBeforeNextRender(index);
    assertActiveViews(defaultViews);

    // Non-exhaustive list of PRIVACY child routes to check.
    // Some of these routs have not been migrated to the new architecture
    // (crbug.com/424223101), therefore the contents still reside in the 'old'
    // <settings-basic-page> view.
    const routesToVisit: Array<{route: Route, viewId: string}> = [
      {route: routes.CLEAR_BROWSER_DATA, viewId: 'old'},
      {route: routes.COOKIES, viewId: 'cookies'},
      {route: routes.SAFETY_HUB, viewId: 'safetyHub'},
      {route: routes.SECURITY, viewId: 'old'},
      {route: routes.SITE_SETTINGS_LOCATION, viewId: 'old'},
      {route: routes.SITE_SETTINGS, viewId: 'old'},
    ];

    for (const {route, viewId} of routesToVisit) {
      Router.getInstance().navigateTo(route);
      await flushTasks();
      await waitBeforeNextRender(index);
      assertActiveViews([viewId]);
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

  test('RoutingSecurityKeys', async function() {
    assertFalse(loadTimeData.getBoolean('enableSecurityKeysSubpage'));
    await createPrivacyPageIndex({enableSecurityKeysSubpage: true});

    Router.getInstance().navigateTo(routes.SECURITY_KEYS);
    await flushTasks();
    await waitBeforeNextRender(index);
    assertActiveViews(['securityKeys']);

    // Test that data-parent-view is correctly populated.
    assertTrue(!!index.$.viewManager.querySelector(
        `#securityKeys[slot=view][data-parent-view-id=old]`));
  });

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
