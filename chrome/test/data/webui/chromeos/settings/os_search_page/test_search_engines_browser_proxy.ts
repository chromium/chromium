// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CategorizedTemplateUrls, SearchEnginesBrowserProxy, SearchEnginesInfo} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSearchEnginesBrowserProxy extends TestBrowserProxy implements
    SearchEnginesBrowserProxy {
  private categorizedTemplateUrls_: CategorizedTemplateUrls = {
    activeSiteShortcuts: [],
    inactiveSiteShortcuts: [],
    activeFeatureShortcuts: [],
    inactiveFeatureShortcuts: [],
  };

  private searchEnginesInfo_: SearchEnginesInfo =
      {defaults: [], actives: [], others: [], extensions: []};

  constructor() {
    super([
      'getCategorizedTemplateUrls',
      'getSearchEnginesList',
      'openBrowserSearchSettings',
    ]);
  }

  getCategorizedTemplateUrls() {
    this.methodCalled('getCategorizedTemplateUrls');
    return Promise.resolve(this.categorizedTemplateUrls_);
  }

  getSearchEnginesList(): Promise<SearchEnginesInfo> {
    this.methodCalled('getSearchEnginesList');
    return Promise.resolve(this.searchEnginesInfo_);
  }

  openBrowserSearchSettings(): void {
    this.methodCalled('openBrowserSearchSettings');
  }

  /**
   * Sets the response to be returned by `getCategorizedTemplateUrls`.
   */
  setCategorizedTemplateUrls(categorizedTemplateUrls: CategorizedTemplateUrls) {
    this.categorizedTemplateUrls_ = categorizedTemplateUrls;
  }

  /**
   * Sets the response to be returned by `getSearchEnginesList`.
   */
  setSearchEnginesInfo(searchEnginesInfo: SearchEnginesInfo) {
    this.searchEnginesInfo_ = searchEnginesInfo;
  }
}
