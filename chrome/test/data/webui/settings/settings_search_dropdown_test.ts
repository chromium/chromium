// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {SearchResult, SettingsSearchDropdownElement} from 'chrome://settings/settings.js';
import {loadTimeData, resetPageVisibilityForTesting, resetRouterForTesting, routes, SettingId, settingsSearchBrowserProxyFactory, SettingsSearchManagerImpl} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createSearchResult, TestSettingsSearchHandler} from './test_settings_search_handler.js';
// clang-format on

suite('SettingsSearchDropdown', () => {
  let dropdown: SettingsSearchDropdownElement;
  let handler: TestSettingsSearchHandler;

  function getRowTitles(): string[] {
    return Array.from(
        dropdown.$.dropdown.querySelectorAll<HTMLElement>('.result-title'),
        el => el.textContent.trim());
  }

  function getRowBreadcrumbs(): string[] {
    return Array.from(
        dropdown.$.dropdown.querySelectorAll<HTMLElement>('.result'), row => {
          const breadcrumbs =
              row.querySelector<HTMLElement>('.result-breadcrumbs');
          return breadcrumbs ? breadcrumbs.textContent.trim() : '';
        });
  }

  function getRowIcons(): string[] {
    return Array.from(
        dropdown.$.dropdown.querySelectorAll<CrIconElement>('.result-icon'),
        el => {
          // No <svg> is rendered if the iconset doesn't have an icon of that
          // name.
          assertTrue(!!el.shadowRoot.querySelector('svg'), el.icon);
          return el.icon;
        });
  }

  async function simulateSearchQuery(query: string) {
    dropdown.query = query;
    await microtasksFinished();
  }

  setup(() => {
    // The dropdown is only shown with SettingsRefresh2026 enabled, see
    // IsSettingsSearch2026Enabled() in chrome/browser/ui/ui_features.cc.
    loadTimeData.overrideValues({settingsRefresh2026: 'settings-refresh-2026'});
    resetPageVisibilityForTesting();
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = new TestSettingsSearchHandler();
    settingsSearchBrowserProxyFactory.setInstance({handler});
    SettingsSearchManagerImpl.resetInstanceForTesting();
    dropdown = document.createElement('settings-search-dropdown');
    document.body.appendChild(dropdown);
  });

  test('DoesNotSearchOnInitialization', async () => {
    const queries: string[] = [];
    SettingsSearchManagerImpl.setInstance({
      search(query: string) {
        queries.push(query);
        return Promise.resolve({query, results: [], canceled: false});
      },
      clear() {},
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dropdown = document.createElement('settings-search-dropdown');
    document.body.appendChild(dropdown);
    await microtasksFinished();
    assertDeepEquals([], queries);
  });

  test('ShowsSearchResults', async () => {
    assertFalse(isVisible(dropdown.$.dropdown));
    assertDeepEquals([], getRowTitles());
    assertDeepEquals([], getRowBreadcrumbs());

    handler.setSearchResults([
      createSearchResult({
        id: SettingId.kAppearance,
        title: 'Appearance',
        breadcrumbs: [],
      }),
      createSearchResult({title: 'Theme', breadcrumbs: ['Appearance']}),
      createSearchResult({
        id: SettingId.kFontSize,
        title: 'Font size',
        breadcrumbs: ['Appearance', 'Customize fonts'],
      }),
    ]);
    await simulateSearchQuery('appearance');
    assertTrue(isVisible(dropdown.$.dropdown));
    assertDeepEquals(['Appearance', 'Theme', 'Font size'], getRowTitles());
    assertDeepEquals(
        ['', 'Appearance', 'Appearance > Customize fonts'],
        getRowBreadcrumbs());
  });

  test('ClearsResultsForShortQuery', async () => {
    handler.setSearchResults([createSearchResult({title: 'Theme'})]);
    await simulateSearchQuery('te');
    assertTrue(isVisible(dropdown.$.dropdown));
    assertDeepEquals(['Theme'], getRowTitles());

    await simulateSearchQuery('t');
    assertFalse(isVisible(dropdown.$.dropdown));
    assertDeepEquals([], getRowTitles());
  });

  test('ReplacesPreviousResults', async () => {
    handler.setSearchResults([createSearchResult({title: 'Theme'})]);
    await simulateSearchQuery('theme');
    assertDeepEquals(['Theme'], getRowTitles());

    handler.setSearchResults(
        [createSearchResult({id: SettingId.kFontSize, title: 'Font size'})]);
    await simulateSearchQuery('font');
    assertDeepEquals(['Font size'], getRowTitles());
  });

  test('KeepsResultsWhileNewerSearchIsPending', async () => {
    handler.setSearchResults([createSearchResult({title: 'Theme'})]);
    await simulateSearchQuery('theme');
    assertDeepEquals(['Theme'], getRowTitles());

    // Starting a newer search cancels the pending one right away, before the
    // newer results arrive.
    handler.setSearchResponsePromise(
        new PromiseResolver<{results: SearchResult[]}>().promise);
    await simulateSearchQuery('font');

    const newerResponse = new PromiseResolver<{results: SearchResult[]}>();
    handler.setSearchResponsePromise(newerResponse.promise);
    await simulateSearchQuery('font size');
    assertDeepEquals(['Theme'], getRowTitles());

    newerResponse.resolve({
      results:
          [createSearchResult({id: SettingId.kFontSize, title: 'Font size'})],
    });
    await microtasksFinished();
    assertDeepEquals(['Font size'], getRowTitles());
  });

  test('ShowsSideNavIconForSectionRoute', async () => {
    handler.setSearchResults(
        [createSearchResult({route: routes.APPEARANCE.path})]);
    await simulateSearchQuery('theme');
    assertDeepEquals(['settings:palette'], getRowIcons());
  });

  test('HidesResultsWithUnknownRoute', async () => {
    handler.setSearchResults([
      createSearchResult({title: 'Theme', route: routes.APPEARANCE.path}),
      createSearchResult({title: 'Gone', route: '/doesNotExist'}),
    ]);
    await simulateSearchQuery('theme');
    assertDeepEquals(['Theme'], getRowTitles());
  });

  test('ShowsSideNavIconForSubpageWithoutIcon', async () => {
    handler.setSearchResults([createSearchResult({route: routes.FONTS.path})]);
    await simulateSearchQuery('font');
    assertDeepEquals(['settings:palette'], getRowIcons());
  });

  test('ShowsSubpageIcon', async () => {
    // These are subpages of PRIVACY, even though their paths don't start with
    // its path.
    handler.setSearchResults([
      createSearchResult({route: routes.CLEAR_BROWSER_DATA.path}),
      createSearchResult({route: routes.COOKIES.path}),
      createSearchResult({route: routes.SECURITY.path}),
    ]);
    await simulateSearchQuery('privacy');
    assertDeepEquals(
        ['cr:delete', 'privacy:cookie', 'privacy:lock'], getRowIcons());
  });

  test('ShowsClosestSubpageIconForNestedSubpageRoute', async () => {
    // SITE_SETTINGS_ALL has no icon of its own, so it uses the icon of
    // SITE_SETTINGS.
    handler.setSearchResults([
      createSearchResult({route: routes.SITE_SETTINGS_ALL.path}),
      createSearchResult({route: routes.SITE_SETTINGS_CAMERA.path}),
    ]);
    await simulateSearchQuery('site');
    assertDeepEquals(['privacy:page-info', 'cr:videocam'], getRowIcons());
  });

  test('ShowsSideNavIconForHomeSubpageRoute', async () => {
    handler.setSearchResults([createSearchResult({route: routes.SYNC.path})]);
    await simulateSearchQuery('sync');
    assertDeepEquals(['settings20:home'], getRowIcons());
  });

  test('ShowsSideNavIconForMovedRoute', async () => {
    // SettingsRefresh2026 moves RESET_DIALOG from under RESET to under GENERAL.
    handler.setSearchResults(
        [createSearchResult({route: routes.RESET_DIALOG.path})]);
    await simulateSearchQuery('reset');
    assertDeepEquals(['cr:settings-filled'], getRowIcons());
  });
});
