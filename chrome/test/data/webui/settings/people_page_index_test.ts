// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import type {SettingsPeoplePageIndexElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, resetRouterForTesting, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
// </if>

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

suite('PeoplePageIndex', function() {
  let index: SettingsPeoplePageIndexElement;
  let browserProxy: TestSyncBrowserProxy;

  async function createPeoplePageIndex(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;
    index = document.createElement('settings-people-page-index');
    index.prefs = settingsPrefs.prefs!;
    document.body.appendChild(index);
    return flushTasks();
  }

  setup(function() {
    loadTimeData.overrideValues({
      replaceSyncPromosWithSignInPromos: false,
    });
    resetRouterForTesting();

    // Set SignedInState.SIGNED_IN otherwise navigating to routes.SYNC_ADVANCED
    // would automatically redirect to routes.SYNC.
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);
    browserProxy.testSyncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };

    Router.getInstance().navigateTo(routes.BASIC);
    return createPeoplePageIndex();
  });

  function assertActiveView(id: string) {
    assertTrue(!!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
    assertFalse(
        !!index.$.viewManager.querySelector(`.active[slot=view]:not(#${id})`));
  }

  test('Routing', async function() {
    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveView('parent');

    Router.getInstance().navigateTo(routes.SYNC);
    await microtasksFinished();
    assertActiveView('sync');

    Router.getInstance().navigateTo(routes.SYNC_ADVANCED);
    await microtasksFinished();
    assertActiveView('syncControls');

    // <if expr="not is_chromeos">
    Router.getInstance().navigateTo(routes.IMPORT_DATA);
    await microtasksFinished();
    assertActiveView('parent');

    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    await microtasksFinished();
    assertActiveView('manageProfile');
    // </if>

    Router.getInstance().navigateTo(routes.PEOPLE);
    await microtasksFinished();
    assertActiveView('parent');
  });

  // Test that the child views are properly annotated.
  test('DataParentViewId', function() {
    const childViewsId = [
      'sync', 'syncControls',
      // <if expr="not is_chromeos">
      'manageProfile',
      // </if>
    ];
    for (const id of childViewsId) {
      assertTrue(!!index.$.viewManager.querySelector(
          `#${id}[slot=view][data-parent-view-id=parent]`));
    }
  });

  // Minimal (non-exhaustive) tests to ensure SearchableViewContainerMixin is
  // inherited correctly.
  test('Search', async function() {
    // Test that search finds results in both parent and child views.
    const result = await index.searchContents('Sync');
    assertFalse(result.canceled);
    assertTrue(result.matchCount >= 2);
    assertFalse(result.wasClearSearch);
  });

  // <if expr="not is_chromeos">
  test('RoutingWithReplaceSyncPromosWithSignInPromos', async function() {
    loadTimeData.overrideValues({
      replaceSyncPromosWithSignInPromos: true,
    });
    resetRouterForTesting();
    await createPeoplePageIndex();

    Router.getInstance().navigateTo(routes.ACCOUNT);
    await microtasksFinished();
    assertActiveView('account');

    Router.getInstance().navigateTo(routes.GOOGLE_SERVICES);
    await microtasksFinished();
    assertActiveView('googleServices');
  });

  // Test that the child views are properly annotated.
  test(
      'DataParentViewIdWithReplaceSyncPromosWithSignInPromos',
      async function() {
        loadTimeData.overrideValues({
          replaceSyncPromosWithSignInPromos: true,
        });
        resetRouterForTesting();
        await createPeoplePageIndex();

        const childViewsId = [
          'account',
          'googleServices',
        ];
        for (const id of childViewsId) {
          assertTrue(!!index.$.viewManager.querySelector(
              `#${id}[slot=view][data-parent-view-id=parent]`));
        }
      });

  test('SearchReplaceSyncPromosWithSigninPromos', async function() {
    loadTimeData.overrideValues({
      replaceSyncPromosWithSignInPromos: true,
    });
    resetRouterForTesting();
    await createPeoplePageIndex();

    // Search for a keyword that is available on the settings pages `/account`
    // and `/googleServices`.
    const result = await index.searchContents('google');
    assertFalse(result.canceled);
    assertTrue(result.matchCount >= 2);
    assertFalse(result.wasClearSearch);
  });

  // Regression test for crbug.com/443268152.
  test(
      'SearchUnavailablePageReplaceSyncPromosWithSigninPromos',
      async function() {
        loadTimeData.overrideValues({
          replaceSyncPromosWithSignInPromos: true,
        });
        browserProxy.testSyncStatus = {
          signedInState: SignedInState.SYNCING,
          statusAction: StatusAction.NO_ACTION,
        };
        resetRouterForTesting();
        await createPeoplePageIndex();

        // Search for a keyword that available on the settings pages `/account`
        // and `/googleServices`, and make sure it does not crash even though
        // the pages do not exist when the user is syncing.
        const result = await index.searchContents('google');
        assertFalse(result.canceled);
        assertFalse(result.wasClearSearch);
      });
  // </if>
});
