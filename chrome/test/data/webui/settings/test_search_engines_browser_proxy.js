// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import { SearchEngine,SearchEnginesBrowserProxy, SearchEnginesInfo} from 'chrome://settings/settings.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';
// clang-format on

/**
 * A test version of SearchEnginesBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 *
 * @implements {SearchEnginesBrowserProxy}
 */
export class TestSearchEnginesBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getSearchEnginesList',
      'removeSearchEngine',
      'searchEngineEditCancelled',
      'searchEngineEditCompleted',
      'searchEngineEditStarted',
      'setDefaultSearchEngine',
      'validateSearchEngineInput',
    ]);

    /** @private {!SearchEnginesInfo} */
    this.searchEnginesInfo_ = {defaults: [], others: [], extensions: []};
  }

  /** @override */
  setDefaultSearchEngine(modelIndex) {
    this.methodCalled('setDefaultSearchEngine', modelIndex);
  }

  /** @override */
  removeSearchEngine(modelIndex) {
    this.methodCalled('removeSearchEngine', modelIndex);
  }

  /** @override */
  searchEngineEditStarted(modelIndex) {
    this.methodCalled('searchEngineEditStarted', modelIndex);
  }

  /** @override */
  searchEngineEditCancelled() {
    this.methodCalled('searchEngineEditCancelled');
  }

  /** @override */
  searchEngineEditCompleted(searchEngine, keyword, queryUrl) {
    this.methodCalled('searchEngineEditCompleted');
  }

  /** @override */
  getSearchEnginesList() {
    this.methodCalled('getSearchEnginesList');
    return Promise.resolve(this.searchEnginesInfo_);
  }

  /** @override */
  validateSearchEngineInput(fieldName, fieldValue) {
    this.methodCalled('validateSearchEngineInput');
    return Promise.resolve(true);
  }

  /**
   * Sets the response to be returned by |getSearchEnginesList|.
   * @param {!SearchEnginesInfo} searchEnginesInfo
   */
  setSearchEnginesInfo(searchEnginesInfo) {
    this.searchEnginesInfo_ = searchEnginesInfo;
  }
}

  /**
   * @param {boolean} canBeDefault
   * @param {boolean} canBeEdited
   * @param {boolean} canBeRemoved
   * @return {!SearchEngine}
   */
export function createSampleSearchEngine(
    canBeDefault, canBeEdited, canBeRemoved) {
  return {
    canBeDefault: canBeDefault,
    canBeEdited: canBeEdited,
    canBeRemoved: canBeRemoved,
    default: false,
    displayName: 'Google',
    iconURL: 'http://www.google.com/favicon.ico',
    id: 0,
    isOmniboxExtension: false,
    keyword: 'google.com',
    modelIndex: 0,
    name: 'Google',
    url: 'https://search.foo.com/search?p=%s',
    urlLocked: false,
  };
}
