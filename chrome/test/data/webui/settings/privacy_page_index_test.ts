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
          isPrivacySandboxRestricted: false,
          isPrivacySandboxRestrictedNoticeEnabled: false,
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

  async function testActiveViewsForRoute(route: Route, viewIds: string[]) {
    Router.getInstance().navigateTo(route);
    await flushTasks();
    await waitBeforeNextRender(index);

    for (const id of viewIds) {
      assertTrue(
          !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
    }
  }

  setup(function() {
    return createPrivacyPageIndex();
  });

  test('Routing', async function() {
    const defaultViews = ['old', 'privacyGuidePromo', 'safetyHubEntryPoint'];

    await testActiveViewsForRoute(routes.PRIVACY, defaultViews);
    await testActiveViewsForRoute(routes.BASIC, defaultViews);

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
      await testActiveViewsForRoute(route, [viewId]);
    }
  });

  // TODO(crbug.com/424223101): Remove this test once <settings-basic-page> is
  // removed.
  test('RoutingLazyRender', async function() {
    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    await flushTasks();
    await waitBeforeNextRender(index);
    assertFalse(!!index.$.viewManager.querySelector('#old'));
    await testActiveViewsForRoute(routes.PRIVACY, ['old']);
  });

  test('RoutingPrivacySandboxRestrictedFalse', async function() {
    await createPrivacyPageIndex({
      isPrivacySandboxRestricted: false,
      isPrivacySandboxRestrictedNoticeEnabled: false,
    });

    // Necessary for the PRIVACY_SANDBOX_MANAGE_TOPICS route to not
    // automatically redirect to its parent.
    index.setPrefValue('privacy_sandbox.m1.topics_enabled', true);

    const routesToVisit: Array<{route: Route, viewId: string}> = [
      {route: routes.PRIVACY_SANDBOX, viewId: 'privacySandbox'},
      {route: routes.PRIVACY_SANDBOX_TOPICS, viewId: 'privacySandboxTopics'},
      {
        route: routes.PRIVACY_SANDBOX_MANAGE_TOPICS,
        viewId: 'privacySandboxManageTopics',
      },
      {route: routes.PRIVACY_SANDBOX_FLEDGE, viewId: 'privacySandboxFledge'},
      {
        route: routes.PRIVACY_SANDBOX_AD_MEASUREMENT,
        viewId: 'privacySandboxAdMeasurement',
      },
    ];

    for (const {route, viewId} of routesToVisit) {
      await testActiveViewsForRoute(route, [viewId]);
    }
  });

  test('RoutingPrivacySandboxRestrictedNoticeEnableTrue', async function() {
    await createPrivacyPageIndex({
      isPrivacySandboxRestricted: true,
      isPrivacySandboxRestrictedNoticeEnabled: true,
    });

    // Necessary for the PRIVACY_SANDBOX_MANAGE_TOPICS route to not
    // automatically redirect to its parent.
    index.setPrefValue('privacy_sandbox.m1.topics_enabled', true);

    const routesToVisit: Array<{route: Route, viewId: string}> = [
      {route: routes.PRIVACY_SANDBOX, viewId: 'privacySandbox'},
      {
        route: routes.PRIVACY_SANDBOX_AD_MEASUREMENT,
        viewId: 'privacySandboxAdMeasurement',
      },
    ];

    for (const {route, viewId} of routesToVisit) {
      await testActiveViewsForRoute(route, [viewId]);
    }
  });

  test('RoutingSecurityKeys', async function() {
    assertFalse(loadTimeData.getBoolean('enableSecurityKeysSubpage'));
    await createPrivacyPageIndex({enableSecurityKeysSubpage: true});
    await testActiveViewsForRoute(routes.SECURITY_KEYS, ['securityKeys']);

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
