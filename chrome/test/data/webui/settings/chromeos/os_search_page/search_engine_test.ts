// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLinkRowElement, Router, SearchEngine, SearchEnginesBrowserProxyImpl, SearchEnginesInfo, SettingsSearchEngineElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {clearBody} from '../utils.js';

import {TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';

suite(`<${SettingsSearchEngineElement.is}>`, () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let searchEngineElement: SettingsSearchEngineElement;
  let browserProxy: TestSearchEnginesBrowserProxy;
  let searchEngineInfo: SearchEnginesInfo;

  function createSampleSearchEngine(props: Partial<SearchEngine>):
      SearchEngine {
    return {
      canBeDefault: true,
      canBeEdited: false,
      canBeRemoved: false,
      default: false,
      displayName: '',
      iconURL: 'http://www.google.com/favicon.ico',
      id: 0,
      isOmniboxExtension: false,
      keyword: 'google.com',
      modelIndex: 0,
      name: '',
      url: 'https://search.foo.com/search?p=%s',
      urlLocked: false,
      ...props,
    };
  }

  function generateSearchEngineInfo(): SearchEnginesInfo {
    const searchEngines0 = createSampleSearchEngine({
      default: true,
      displayName: 'SearchEngine0',
      modelIndex: 0,
      name: 'SearchEngine0',
    });
    const searchEngines1 = createSampleSearchEngine({
      displayName: 'SearchEngine1',
      modelIndex: 1,
      name: 'SearchEngine1',
    });

    return {
      defaults: [searchEngines0, searchEngines1],
      actives: [],
      others: [],
      extensions: [],
    };
  }

  setup(async () => {
    loadTimeData.overrideValues({
      isQuickAnswersSupported: false,
    });

    searchEngineInfo = generateSearchEngineInfo();
    browserProxy = new TestSearchEnginesBrowserProxy(searchEngineInfo);
    SearchEnginesBrowserProxyImpl.setInstanceForTesting(browserProxy);

    clearBody();
    searchEngineElement =
        document.createElement(SettingsSearchEngineElement.is);
    document.body.appendChild(searchEngineElement);
    await browserProxy.whenCalled('getSearchEnginesList');
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  function queryLinkRow(): CrLinkRowElement|null {
    return searchEngineElement.shadowRoot!.querySelector<CrLinkRowElement>(
        '#browserSearchSettingsLink');
  }

  test('Search engine settings row displays correctly', () => {
    const linkRow = queryLinkRow();
    assertTrue(!!linkRow);
    assertTrue(linkRow.external);

    if (isRevampWayfindingEnabled) {
      assertEquals(
          'Set search engine in Chrome browser settings', linkRow.label);
    } else {
      assertEquals('Preferred search engine', linkRow.label);
    }

    // Sublabel should display the default search engine name.
    assertEquals('SearchEngine0', linkRow.subLabel);
  });

  test('Clicking row navigates to search engine settings', async () => {
    const linkRow = queryLinkRow();
    assertTrue(!!linkRow);
    linkRow.click();

    await browserProxy.whenCalled('openBrowserSearchSettings');
  });
});
