// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {SearchResult} from 'chrome://settings/settings.js';
import {MAX_SEARCH_RESULTS, SettingId, settingsSearchBrowserProxyFactory, SettingsSearchManagerImpl} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createSearchResult, TestSettingsSearchHandler} from './test_settings_search_handler.js';
// clang-format on

suite('SettingsSearchManager', () => {
  let handler: TestSettingsSearchHandler;
  let searchManager: SettingsSearchManagerImpl;

  setup(() => {
    handler = new TestSettingsSearchHandler();
    settingsSearchBrowserProxyFactory.setInstance({handler});
    searchManager = new SettingsSearchManagerImpl();
  });

  test('QueriesHandlerWithTrimmedQuery', async () => {
    const results = [createSearchResult()];
    handler.setSearchResults(results);

    const request = await searchManager.search('  theme  ');
    assertDeepEquals(
        [['theme', MAX_SEARCH_RESULTS]], handler.getArgs('search'));
    assertEquals('theme', request.query);
    assertDeepEquals(results, request.results);
    assertFalse(request.canceled);
  });

  test('ShortQueriesDoNotQueryHandler', async () => {
    const request = await searchManager.search('  t  ');
    assertEquals(0, handler.getCallCount('search'));
    assertDeepEquals([], request.results);
    assertFalse(request.canceled);
  });

  test('ShortQueryCancelsPendingRequest', async () => {
    handler.setSearchResponsePromise(
        new PromiseResolver<{results: SearchResult[]}>().promise);
    const pendingSearch = searchManager.search('fonts');

    await searchManager.search('f');
    assertTrue((await pendingSearch).canceled);
  });

  test('DuplicateQueriesReuseRequest', async () => {
    const results = [createSearchResult()];
    handler.setSearchResults(results);

    searchManager.search('theme');
    const request = await searchManager.search('theme ');
    assertDeepEquals(results, request.results);
    assertEquals(1, handler.getCallCount('search'));

    // A repeated query after the results arrived reuses them too.
    assertEquals(request, await searchManager.search('theme'));
    assertEquals(1, handler.getCallCount('search'));

    // The query is sent again after a different query.
    await searchManager.search('t');
    await searchManager.search('theme');
    assertEquals(2, handler.getCallCount('search'));
  });

  test('NewerSearchCancelsPendingRequest', async () => {
    const staleResults = [createSearchResult({id: SettingId.kFontSize})];
    const latestResults = [createSearchResult()];

    const firstResponse = new PromiseResolver<{results: SearchResult[]}>();
    handler.setSearchResponsePromise(firstResponse.promise);
    const firstSearch = searchManager.search('fo');

    const secondResponse = new PromiseResolver<{results: SearchResult[]}>();
    handler.setSearchResponsePromise(secondResponse.promise);
    const secondSearch = searchManager.search('theme');
    assertDeepEquals(
        [['fo', MAX_SEARCH_RESULTS], ['theme', MAX_SEARCH_RESULTS]],
        handler.getArgs('search'));

    // The first request resolves as canceled without waiting for its response.
    const firstRequest = await firstSearch;
    assertTrue(firstRequest.canceled);
    assertDeepEquals([], firstRequest.results);

    secondResponse.resolve({results: latestResults});
    const secondRequest = await secondSearch;
    assertFalse(secondRequest.canceled);
    assertDeepEquals(latestResults, secondRequest.results);

    // A late response for the canceled request is ignored.
    firstResponse.resolve({results: staleResults});
    await microtasksFinished();
    assertDeepEquals([], firstRequest.results);
  });

  test('ClearCancelsPendingRequest', async () => {
    const response = new PromiseResolver<{results: SearchResult[]}>();
    handler.setSearchResponsePromise(response.promise);
    const search = searchManager.search('fonts');

    searchManager.clear();
    const request = await search;
    assertTrue(request.canceled);

    const staleResults = [createSearchResult({id: SettingId.kFontSize})];
    response.resolve({results: staleResults});
    await microtasksFinished();
    assertDeepEquals([], request.results);
  });

  test('ClearKeepsCompletedRequest', async () => {
    const results = [createSearchResult()];
    handler.setSearchResults(results);
    const request = await searchManager.search('theme');

    searchManager.clear();
    assertFalse(request.canceled);
    assertDeepEquals(results, request.results);
  });

  test('HandlerFailureCancelsRequest', async () => {
    handler.setSearchResponsePromise(Promise.reject(new Error('Pipe closed')));
    const request = await searchManager.search('theme');
    assertTrue(request.canceled);

    // A repeated query sends a new request.
    handler.setSearchResults([]);
    await searchManager.search('theme');
    assertEquals(2, handler.getCallCount('search'));
  });
});
