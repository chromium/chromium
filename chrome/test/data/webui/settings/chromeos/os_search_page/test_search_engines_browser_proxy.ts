// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchEnginesBrowserProxy, SearchEnginesInfo} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSearchEnginesBrowserProxy extends TestBrowserProxy implements
    SearchEnginesBrowserProxy {
  constructor(private searchEngineInfo: SearchEnginesInfo) {
    super([
      'getSearchEnginesList',
      'openBrowserSearchSettings',
    ]);
  }

  getSearchEnginesList(): Promise<SearchEnginesInfo> {
    this.methodCalled('getSearchEnginesList');
    return Promise.resolve(this.searchEngineInfo);
  }

  openBrowserSearchSettings(): void {
    this.methodCalled('openBrowserSearchSettings');
  }
}
