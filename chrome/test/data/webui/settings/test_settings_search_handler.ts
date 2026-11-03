// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SearchResult, SettingsSearchHandlerInterface} from 'chrome://settings/settings.js';
import {SettingId} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export function createSearchResult(override?: Partial<SearchResult>):
    SearchResult {
  return Object.assign(
      {
        id: SettingId.kThemes,
        route: '/appearance',
        title: 'Theme',
        breadcrumbs: ['Appearance'],
        score: 1,
        titleMatchRanges: [],
      },
      override);
}

export class TestSettingsSearchHandler extends TestBrowserProxy implements
    SettingsSearchHandlerInterface {
  private results_: SearchResult[] = [];
  private responsePromise_: Promise<{results: SearchResult[]}>|null = null;

  constructor() {
    super(['search']);
  }

  setSearchResults(results: SearchResult[]) {
    this.results_ = results;
    this.responsePromise_ = null;
  }

  /**
   * Makes search() return `response`, so that tests can control when (or
   * whether) a response arrives. Takes precedence over setSearchResults()
   * until setSearchResults() is called again.
   */
  setSearchResponsePromise(response: Promise<{results: SearchResult[]}>) {
    this.responsePromise_ = response;
  }

  search(query: string, maxResults: number):
      Promise<{results: SearchResult[]}> {
    this.methodCalled('search', query, maxResults);
    return this.responsePromise_ ?? Promise.resolve({results: this.results_});
  }
}
