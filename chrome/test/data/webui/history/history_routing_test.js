// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, BrowserService, PageCallbackRouter, PageHandlerRemote} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

function createHistoryClustersBrowserProxy() {
  const handler = TestBrowserProxy.fromClass(PageHandlerRemote);
  const callbackRouter = new PageCallbackRouter();
  BrowserProxy.setInstance(new BrowserProxy(handler, callbackRouter));
}

[true, false].forEach(isHistoryClustersEnabled => {
  suite('routing-test', function() {
    let app;
    let list;
    let sidebar;
    let toolbar;

    function navigateTo(route) {
      window.history.replaceState({}, '', route);
      window.dispatchEvent(new CustomEvent('location-changed'));
      // Update from the URL synchronously.
      app.$$('history-router').debouncer_.flush();
    }

    suiteSetup(() => {
      loadTimeData.overrideValues({isHistoryClustersEnabled});
    });

    setup(function() {
      window.history.replaceState({}, '', '/');
      document.body.innerHTML = '';
      BrowserService.setInstance(new TestBrowserService());
      createHistoryClustersBrowserProxy();
      app = document.createElement('history-app');
      document.body.appendChild(app);

      assertEquals('chrome://history/', window.location.href);
      sidebar = app.$['content-side-bar'];
      toolbar = app.$['toolbar'];
      return flushTasks();
    });

    test('history clusters menu item visibility', function() {
      const historyClusters =
          sidebar.shadowRoot.querySelector('#historyClusters');
      assertEquals(!!historyClusters, isHistoryClustersEnabled);
    });

    test('changing route changes active view', function() {
      assertEquals('history', app.$.content.selected);
      navigateTo('/syncedTabs');
      return flushTasks().then(function() {
        assertEquals('syncedTabs', app.$.content.selected);
        assertEquals('chrome://history/syncedTabs', window.location.href);
      });
    });

    test('changing route to /journeys may change active view', function() {
      assertEquals('history', app.$.content.selected);
      navigateTo('/journeys');
      return flushTasks().then(function() {
        assertEquals(
            isHistoryClustersEnabled ? 'journeys' : 'history',
            app.$.content.selected);
        assertEquals('chrome://history/journeys', window.location.href);
      });
    });

    test('route updates from sidebar', function() {
      assertEquals('history', app.selectedPage_);
      assertEquals('chrome://history/', window.location.href);

      sidebar.$.syncedTabs.click();
      assertEquals('syncedTabs', app.selectedPage_);
      assertEquals('chrome://history/syncedTabs', window.location.href);

      keyDownOn(sidebar.$.history, 32, '', ' ');
      assertEquals('history', app.selectedPage_);
      assertEquals('chrome://history/', window.location.href);

      if (isHistoryClustersEnabled) {
        const historyClusters =
            sidebar.shadowRoot.querySelector('#historyClusters');
        keyDownOn(historyClusters, 32, '', ' ');
        assertEquals('journeys', app.selectedPage_);
        assertEquals('chrome://history/journeys', window.location.href);
      }
    });

    test('search updates from route', function() {
      assertEquals('chrome://history/', window.location.href);
      const searchTerm = 'Mei';
      assertEquals('history', app.$.content.selected);
      navigateTo('/?q=' + searchTerm);
      assertEquals(searchTerm, toolbar.searchTerm);
    });

    test('route updates from search', function() {
      const searchTerm = 'McCree';
      assertEquals('history', app.$.content.selected);
      app.fire('change-query', {search: searchTerm});
      assertEquals('chrome://history/?q=' + searchTerm, window.location.href);
    });

    test('search preserved across menu items', function() {
      const searchTerm = 'Soldier76';
      const menu = sidebar.$.menu;
      assertEquals('history', app.selectedPage_);
      navigateTo('/?q=' + searchTerm);

      sidebar.$.syncedTabs.click();
      assertEquals('syncedTabs', app.selectedPage_);
      assertEquals(searchTerm, toolbar.searchTerm);
      assertEquals(
          'chrome://history/syncedTabs?q=' + searchTerm, window.location.href);

      sidebar.$.history.click();
      assertEquals('history', app.selectedPage_);
      assertEquals(searchTerm, toolbar.searchTerm);
      assertEquals('chrome://history/?q=' + searchTerm, window.location.href);

      if (isHistoryClustersEnabled) {
        sidebar.shadowRoot.querySelector('#historyClusters').click();
        assertEquals('journeys', app.selectedPage_);
        assertEquals(searchTerm, toolbar.searchTerm);
        assertEquals(
            'chrome://history/journeys?q=' + searchTerm, window.location.href);
      }
    });
  });
});
