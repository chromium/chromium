// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {SearchResult, SettingsSearchDropdownElement} from 'chrome://settings/settings.js';
import {SettingId, settingsSearchBrowserProxyFactory, SettingsSearchManagerImpl} from 'chrome://settings/settings.js';
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

  async function simulateSearchQuery(query: string) {
    dropdown.query = query;
    await microtasksFinished();
  }

  setup(() => {
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
});
