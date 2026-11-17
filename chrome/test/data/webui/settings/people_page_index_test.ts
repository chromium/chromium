// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {DeviceTabVisibilityBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsPeoplePageIndexElement} from 'chrome://settings/settings.js';
import {loadTimeData, PrefsBrowserProxy, PrefService, resetRouterForTesting, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestDeviceTabVisibilityBrowserProxy} from './test_device_tab_visibility_browser_proxy.js';
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

    let whenEntered: Promise<Event>;

    // <if expr="not is_chromeos">
    Router.getInstance().navigateTo(routes.IMPORT_DATA);
    assertActiveView('parent');

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    await whenEntered;
    assertActiveView('manageProfile');
    // </if>

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.ACCOUNT);
    await whenEntered;
    assertActiveView('account');

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.GOOGLE_SERVICES);
    await whenEntered;
    assertActiveView('googleServices');

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.PEOPLE);
    await whenEntered;
    assertActiveView('parent');

    browserProxy.testSyncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    webUIListenerCallback('sync-status-changed', browserProxy.testSyncStatus);
    await microtasksFinished();

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.SYNC);
    await whenEntered;
    assertActiveView('sync');

    whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.SYNC_ADVANCED);
    await whenEntered;
    assertActiveView('syncControls');
  });

  // Test that the child views are properly annotated.
  test('DataParentViewId', function() {
    const childViewsId = [
      'sync',
      'syncControls',
      'account',
      'googleServices',
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

    // Search for a keyword that is available on the settings pages `/account`
    // and `/googleServices`.
    const googleResult = await index.searchContents('google');
    assertFalse(googleResult.canceled);
    assertTrue(googleResult.matchCount >= 2);
    assertFalse(googleResult.wasClearSearch);
  });

  // Regression test for crbug.com/443268152.
  test('SearchUnavailablePage', async function() {
    browserProxy.testSyncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    await createPeoplePageIndex();

    // Search for a keyword that available on the settings pages `/account`
    // and `/googleServices`, and make sure it does not crash even though
    // the pages do not exist when the user is syncing.
    const result = await index.searchContents('google');
    assertFalse(result.canceled);
    assertFalse(result.wasClearSearch);
  });

  // Verifies that navigating to DEVICE_TAB_VISIBILITY activates the
  // deviceTabVisibility view in people-page-index when the feature is enabled.
  test('RoutingWithDeviceTabVisibilitySettings', async function() {
    const viewId = 'deviceTabVisibility';
    // Use the test browser proxy and enable the feature before initializing
    // the router and view manager.
    DeviceTabVisibilityBrowserProxyImpl.setInstance(
        new TestDeviceTabVisibilityBrowserProxy());
    loadTimeData.overrideValues({
      enableDeviceTabVisibilitySettings: true,
    });
    resetRouterForTesting();
    await createPeoplePageIndex();

    // The `deviceTabVisibility` subpage view is registered in the view manager
    // under the `parent` view.
    assertTrue(!!index.$.viewManager.querySelector(
        `#${viewId}[slot=view][data-parent-view-id=parent]`));

    // Navigating to `routes.DEVICE_TAB_VISIBILITY` switches the active view.
    const whenEntered = eventToPromise('view-enter-finish', index);
    Router.getInstance().navigateTo(routes.DEVICE_TAB_VISIBILITY);
    await whenEntered;
    assertActiveView(viewId);
  });
});
