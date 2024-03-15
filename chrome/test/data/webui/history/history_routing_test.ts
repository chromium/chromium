// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement, HistorySideBarElement} from 'chrome://history/history.js';
import {BrowserProxyImpl, BrowserServiceImpl, HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote, MetricsProxyImpl} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestBrowserProxy, TestMetricsProxy} from './history_clusters/utils.js';
import {TestBrowserService} from './test_browser_service.js';
import {navigateTo} from './test_util.js';

[true, false].forEach(isHistoryClustersEnabled => {
  const suitSuffix = isHistoryClustersEnabled ? 'enabled' : 'disabled';
  suite(`routing-test-with-history-clusters-${suitSuffix}`, function() {
    let app: HistoryAppElement;
    let sidebar: HistorySideBarElement;
    let testBrowserProxy: TestBrowserProxy;
    let testMetricsProxy: TestMetricsProxy;

    suiteSetup(() => {
      loadTimeData.overrideValues({
        disableHistoryClusters: 'Disable',
        enableHistoryClusters: 'Enable',
        isHistoryClustersEnabled,
        isHistoryClustersVisible: true,
      });
    });

    setup(function() {
      window.history.replaceState({}, '', '/');
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      BrowserServiceImpl.setInstance(new TestBrowserService());
      testBrowserProxy = new TestBrowserProxy();
      BrowserProxyImpl.setInstance(testBrowserProxy);
      testMetricsProxy = new TestMetricsProxy();
      MetricsProxyImpl.setInstance(testMetricsProxy);
      app = document.createElement('history-app');
      document.body.appendChild(app);

      assertEquals('chrome://history/', window.location.href);
      sidebar = app.$['content-side-bar'];
      return flushTasks();
    });

    test('changing route changes active view', function() {
      assertEquals('history', app.$.content.selected);
      assertEquals(app.$.history, app.$['tabs-content'].selectedItem);

      navigateTo('/syncedTabs', app);
      return flushTasks().then(function() {
        assertEquals('chrome://history/syncedTabs', window.location.href);

        assertEquals('syncedTabs', app.$.content.selected);
        assertEquals(
            app.shadowRoot!.querySelector('#synced-devices'),
            app.$.content.selectedItem);
      });
    });

    test('routing to /grouped may change active view', function() {
      assertEquals('history', app.$.content.selected);
      assertEquals(
          app.shadowRoot!.querySelector('#history'),
          app.$['tabs-content'].selectedItem);

      navigateTo('/grouped', app);
      return flushTasks().then(function() {
        assertEquals('chrome://history/grouped', window.location.href);

        assertEquals('history', app.$.content.selected);
        assertEquals(
            !!app.shadowRoot!.querySelector('#history-clusters'),
            isHistoryClustersEnabled);
        assertEquals(
            isHistoryClustersEnabled ?
                app.shadowRoot!.querySelector('#history-clusters') :
                app.shadowRoot!.querySelector('#history'),
            app.$['tabs-content'].selectedItem);
      });
    });

    test('routing to /grouped may update sidebar menu item', function() {
      assertEquals('chrome://history/', sidebar.$.history.href);
      assertEquals('history', sidebar.$.history.getAttribute('path'));

      navigateTo('/grouped', app);
      return flushTasks().then(function() {
        // Currently selected history view is preserved in sidebar menu item.
        assertEquals(
            isHistoryClustersEnabled ? 'chrome://history/grouped' :
                                       'chrome://history/',
            sidebar.$.history.href);
        assertEquals(
            isHistoryClustersEnabled ? 'grouped' : 'history',
            sidebar.$.history.getAttribute('path'));
      });
    });

    test('route updates from tabs and sidebar menu items', async function() {
      assertEquals('history', sidebar.$.menu.selected);
      assertEquals('chrome://history/', window.location.href);

      sidebar.$.syncedTabs.click();
      assertEquals('syncedTabs', sidebar.$.menu.selected);
      assertEquals('chrome://history/syncedTabs', window.location.href);

      // Currently selected history view is preserved in sidebar menu item.
      keyDownOn(sidebar.$.history, 0, '', ' ');
      assertEquals('history', sidebar.$.menu.selected);
      assertEquals('chrome://history/', window.location.href);

      const historyTabs = app.shadowRoot!.querySelector('cr-tabs');
      assertEquals(!!historyTabs, isHistoryClustersEnabled);

      if (isHistoryClustersEnabled) {
        assertTrue(!!historyTabs);
        historyTabs.selected = 1;
        await historyTabs.updateComplete;
        assertEquals('grouped', sidebar.$.menu.selected);
        assertEquals('chrome://history/grouped', window.location.href);

        keyDownOn(sidebar.$.syncedTabs, 0, '', ' ');
        assertEquals('syncedTabs', sidebar.$.menu.selected);
        assertEquals('chrome://history/syncedTabs', window.location.href);

        // Currently selected history view is preserved in sidebar menu item.
        keyDownOn(sidebar.$.history, 0, '', ' ');
        assertEquals('grouped', sidebar.$.menu.selected);
        assertEquals('chrome://history/grouped', window.location.href);

        historyTabs.selected = 0;
        await historyTabs.updateComplete;
        assertEquals('history', sidebar.$.menu.selected);
        assertEquals('chrome://history/', window.location.href);
      }
    });

    test('search updates from route', function() {
      assertEquals('chrome://history/', window.location.href);
      const searchTerm = 'Mei';
      assertEquals('history', app.$.content.selected);
      navigateTo('/?q=' + searchTerm, app);
      assertEquals(searchTerm, app.$.toolbar.searchTerm);
    });

    test('route updates from search', function() {
      const searchTerm = 'McCree';
      assertEquals('history', app.$.content.selected);
      app.dispatchEvent(new CustomEvent(
          'change-query',
          {bubbles: true, composed: true, detail: {search: searchTerm}}));
      assertEquals('chrome://history/?q=' + searchTerm, window.location.href);
    });

    test(
        'search is preserved across tabs and sidebar menu items',
        async function() {
          const searchTerm = 'Soldier76';
          assertEquals('history', sidebar.$.menu.selected);
          navigateTo('/?q=' + searchTerm, app);

          sidebar.$.syncedTabs.click();
          assertEquals('syncedTabs', sidebar.$.menu.selected);
          assertEquals(searchTerm, app.$.toolbar.searchTerm);
          assertEquals(
              'chrome://history/syncedTabs?q=' + searchTerm,
              window.location.href);

          sidebar.$.history.click();
          assertEquals('history', sidebar.$.menu.selected);
          assertEquals(searchTerm, app.$.toolbar.searchTerm);
          assertEquals(
              'chrome://history/?q=' + searchTerm, window.location.href);

      if (isHistoryClustersEnabled) {
        const tabs = app.shadowRoot!.querySelector('cr-tabs');
        assertTrue(!!tabs);
        tabs.selected = 1;
        await tabs.updateComplete;
        assertEquals('grouped', sidebar.$.menu.selected);
        assertEquals(searchTerm, app.$.toolbar.searchTerm);
        assertEquals(
            'chrome://history/grouped?q=' + searchTerm, window.location.href);
      }
    });
  });
});

suite(`routing-test-with-history-clusters-pref-set`, () => {
  let app: HistoryAppElement;
  let testBrowserProxy: TestBrowserProxy;
  let testMetricsProxy: TestMetricsProxy;
  let testBrowserService: TestBrowserService;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isHistoryClustersEnabled: true,
      isHistoryClustersVisible: true,
      lastSelectedTab: 1,
    });
  });

  setup(function() {
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testBrowserService);
    testBrowserProxy = new TestBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);
    testMetricsProxy = new TestMetricsProxy();
    MetricsProxyImpl.setInstance(testMetricsProxy);

    return flushTasks();
  });

  async function initialize() {
    app = document.createElement('history-app');
    document.body.appendChild(app);
  }

  test(
      `route to non default last selected tab when no url params set `,
      async () => {
        initialize();
        assertEquals(`chrome://history/grouped`, window.location.href);
      });

  test(`route to grouped url when last tab is grouped`, async () => {
    initialize();
    assertEquals(`chrome://history/grouped`, window.location.href);
    navigateTo('/grouped', app);
    assertEquals(`chrome://history/grouped`, window.location.href);
    const lastSelectedTab =
        await testBrowserService.whenCalled('setLastSelectedTab');
    assertEquals(lastSelectedTab, 1);
  });

  test(`route to list url when last tab is list`, async () => {
    loadTimeData.overrideValues({lastSelectedTab: 0});
    initialize();
    assertEquals(`chrome://history/`, window.location.href);
  });
});

suite(`routing-test-with-history-embeddings-enabled`, () => {
  let app: HistoryAppElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      enableHistoryEmbeddings: true,
      isHistoryClustersEnabled: true,
      isHistoryClustersVisible: true,
      historyEmbeddingsSuggestion1: 'suggestion 1',
      historyEmbeddingsSuggestion2: 'suggestion 2',
      historyEmbeddingsSuggestion3: 'suggestion 3',
    });
  });

  setup(() => {
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Some extra setup of mocking proxies to get the history-app to work.
    BrowserServiceImpl.setInstance(new TestBrowserService());
    BrowserProxyImpl.setInstance(new TestBrowserProxy());
    MetricsProxyImpl.setInstance(new TestMetricsProxy());
    const handler = TestMock.fromClass(HistoryEmbeddingsPageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(handler));
    handler.setResultFor('doSomething', Promise.resolve(true));

    app = document.createElement('history-app');
    document.body.appendChild(app);
    return flushTasks();
  });

  test('route updates from filter chips', () => {
    // Tabs should be hidden.
    assertEquals(null, app.shadowRoot!.querySelector('cr-tabs'));

    const filterChips =
        app.shadowRoot!.querySelector('cr-history-embeddings-filter-chips');
    assertTrue(!!filterChips);
    assertTrue(isVisible(filterChips));

    // Changing the "By group" chip to should change the URL.
    filterChips.dispatchEvent(new CustomEvent(
        'show-results-by-group-changed', {detail: {value: true}}));
    assertEquals('chrome://history/grouped', window.location.href);

    filterChips.dispatchEvent(new CustomEvent(
        'show-results-by-group-changed', {detail: {value: false}}));
    assertEquals('chrome://history/', window.location.href);
  });
});
