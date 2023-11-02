// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesInfo, SearchEnginesInteractions} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/**
 * A test version of SearchEnginesBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
export class TestSearchEnginesBrowserProxy extends TestBrowserProxy implements
    SearchEnginesBrowserProxy {
  private searchEnginesInfo_: SearchEnginesInfo;

  constructor() {
    super([
      'getSearchEnginesList',
      'removeSearchEngine',
      'searchEngineEditCancelled',
      'searchEngineEditCompleted',
      'searchEngineEditStarted',
      'setDefaultSearchEngine',
      'setIsActiveSearchEngine',
      'validateSearchEngineInput',
      'recordSearchEnginesPageHistogram',
    ]);

    this.searchEnginesInfo_ =
        {defaults: [], actives: [], others: [], extensions: []};
  }

  setDefaultSearchEngine(modelIndex: number) {
    this.methodCalled('setDefaultSearchEngine', modelIndex);
  }

  setIsActiveSearchEngine(modelIndex: number, isActive: boolean) {
    this.methodCalled('setIsActiveSearchEngine', [modelIndex, isActive]);
  }

  removeSearchEngine(modelIndex: number) {
    this.methodCalled('removeSearchEngine', modelIndex);
  }

  searchEngineEditStarted(modelIndex: number) {
    this.methodCalled('searchEngineEditStarted', modelIndex);
  }

  searchEngineEditCancelled() {
    this.methodCalled('searchEngineEditCancelled');
  }

  searchEngineEditCompleted(
      searchEngine: string, keyword: string, queryUrl: string) {
    this.methodCalled(
        'searchEngineEditCompleted', [searchEngine, keyword, queryUrl]);
  }

  getSearchEnginesList() {
    this.methodCalled('getSearchEnginesList');
    return Promise.resolve(this.searchEnginesInfo_);
  }

  validateSearchEngineInput(fieldName: string, fieldValue: string) {
    this.methodCalled('validateSearchEngineInput', [fieldName, fieldValue]);
    return Promise.resolve(true);
  }

  recordSearchEnginesPageHistogram(interaction: SearchEnginesInteractions) {
    this.methodCalled('recordSearchEnginesPageHistogram', interaction);
  }

  /**
   * Sets the response to be returned by |getSearchEnginesList|.
   */
  setSearchEnginesInfo(searchEnginesInfo: SearchEnginesInfo) {
    this.searchEnginesInfo_ = searchEnginesInfo;
  }
}

export function createSampleSearchEngine(override?: Partial<SearchEngine>):
    SearchEngine {
  return Object.assign(
      {
        canBeDefault: false,
        canBeEdited: false,
        canBeRemoved: false,
        canBeActivated: false,
        canBeDeactivated: false,
        default: false,
        displayName: 'Google',
        iconURL: 'http://www.google.com/favicon.ico',
        id: 0,
        isOmniboxExtension: false,
        keyword: 'google.com',
        modelIndex: 0,
        name: 'Google',
        shouldConfirmDeletion: false,
        url: 'https://search.foo.com/search?p=%s',
        urlLocked: false,
      },
      override || {});
}
