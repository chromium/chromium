// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import type {SettingsPeoplePageIndexElement} from 'chrome://settings/settings.js';
import {loadTimeData, PrefsBrowserProxy, PrefService, resetRouterForTesting, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: 'signin.allowed_on_next_startup',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'import_dialog_autofill_form_data',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'import_dialog_bookmarks',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'import_dialog_history',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'import_dialog_saved_passwords',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'import_dialog_search_engine',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'bookmark_bar.show_on_all_tabs',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'search.suggest_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'url_keyed_anonymized_data_collection.enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'spellcheck.use_spelling_service',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'spellcheck.dictionaries',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: ['en-US'],
    },
  ];
}

suite('PeoplePageIndex', function() {
  let index: SettingsPeoplePageIndexElement;
  let browserProxy: TestSyncBrowserProxy;

  async function createPeoplePageIndex(): Promise<void> {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    index = document.createElement('settings-people-page-index');
    const whenViewEntered = eventToPromise('view-enter-finish', index);
    document.body.appendChild(index);
    await whenViewEntered;
  }

  setup(async function() {
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
    await createPeoplePageIndex();
  });

  function assertActiveView(id: string) {
    assertTrue(!!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
    assertFalse(
        !!index.$.viewManager.querySelector(`.active[slot=view]:not(#${id})`));
  }

  test('Routing', async function() {
    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveView('parent');

    let whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.SYNC);
    await whenEntered;
    assertActiveView('sync');

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.SYNC_ADVANCED);
    await whenEntered;
    assertActiveView('syncControls');

    // <if expr="not is_chromeos">
    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.IMPORT_DATA);
    await whenEntered;
    assertActiveView('parent');

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    await whenEntered;
    assertActiveView('manageProfile');
    // </if>

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.PEOPLE);
    await whenEntered;
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

  test('RoutingWithReplaceSyncPromosWithSignInPromos', async function() {
    loadTimeData.overrideValues({
      replaceSyncPromosWithSignInPromos: true,
    });
    resetRouterForTesting();
    await createPeoplePageIndex();

    let whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.ACCOUNT);
    await whenEntered;
    assertActiveView('account');

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.GOOGLE_SERVICES);
    await whenEntered;
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
});
