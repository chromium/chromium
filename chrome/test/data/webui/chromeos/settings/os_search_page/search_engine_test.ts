// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CategorizedTemplateUrls, CrLinkRowElement, SearchEngine, SearchEnginesInfo} from 'chrome://os-settings/os_settings.js';
import {Router, SearchEnginesBrowserProxyImpl, SettingsSearchEngineElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../utils.js';

import {TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';

function createSampleSearchEngine(props: Partial<SearchEngine>): SearchEngine {
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
    name: '',
    url: 'https://search.foo.com/search?p=%s',
    urlLocked: false,
    ...props,
  };
}

suite(`<${SettingsSearchEngineElement.is}>`, () => {
  let searchEngineElement: SettingsSearchEngineElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  function generateSearchEngineInfo(): SearchEnginesInfo {
    const searchEngines0 = createSampleSearchEngine({
      default: true,
      displayName: 'SearchEngine0',
      id: 0,
      name: 'SearchEngine0',
    });
    const searchEngines1 = createSampleSearchEngine({
      displayName: 'SearchEngine1',
      id: 1,
      name: 'SearchEngine1',
    });

    return {
      defaults: [searchEngines0, searchEngines1],
      actives: [],
      others: [],
      extensions: [],
    };
  }

  setup(() => {
    loadTimeData.overrideValues({
      searchSettingsUpdate: false,
      isQuickAnswersSupported: false,
    });

    browserProxy = new TestSearchEnginesBrowserProxy();
    const searchEngineInfo = generateSearchEngineInfo();
    browserProxy.setSearchEnginesInfo(searchEngineInfo);
    SearchEnginesBrowserProxyImpl.setInstanceForTesting(browserProxy);

    clearBody();
    searchEngineElement =
        document.createElement(SettingsSearchEngineElement.is);
    document.body.appendChild(searchEngineElement);
    webUIListenerCallback('search-engines-changed', searchEngineInfo);
    return flushTasks();
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

    assertEquals('Set search engine in Chrome browser settings', linkRow.label);

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

suite(`SearchPageTestWithSearchSettingsUpdate`, () => {
  let searchEngineElement: SettingsSearchEngineElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  function generateCategorizedTemplateUrls(): CategorizedTemplateUrls {
    const searchEngines0 = createSampleSearchEngine({
      default: true,
      displayName: 'SearchEngine0',
      id: 0,
      name: 'SearchEngine0',
    });
    const searchEngines1 = createSampleSearchEngine({
      displayName: 'SearchEngine1',
      id: 1,
      name: 'SearchEngine1',
    });

    return {
      activeSiteShortcuts: [searchEngines0, searchEngines1],
      inactiveSiteShortcuts: [],
      activeFeatureShortcuts: [],
      inactiveFeatureShortcuts: [],
    };
  }

  setup(() => {
    loadTimeData.overrideValues({
      searchSettingsUpdate: true,
      isQuickAnswersSupported: false,
    });

    browserProxy = new TestSearchEnginesBrowserProxy();
    const categorizedData = generateCategorizedTemplateUrls();
    browserProxy.setCategorizedTemplateUrls(categorizedData);
    SearchEnginesBrowserProxyImpl.setInstanceForTesting(browserProxy);

    clearBody();
    searchEngineElement =
        document.createElement(SettingsSearchEngineElement.is);
    document.body.appendChild(searchEngineElement);
    webUIListenerCallback('search-engines-changed', categorizedData);
    return flushTasks();
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

    assertEquals('Set search engine in Chrome browser settings', linkRow.label);

    // Sublabel should display the default search engine name.
    assertEquals('SearchEngine0', linkRow.subLabel);
  });
});
