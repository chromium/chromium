// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, BrowserService, PageCallbackRouter, PageHandlerRemote} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {flushTasks} from 'chrome://test/test_util.js';

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

    test('changing route changes active view', function() {
      assertEquals('history', app.$.content.selected);
      assertEquals(app.$$('#history'), app.$['tabs-content'].selectedItem);

      navigateTo('/syncedTabs');
      return flushTasks().then(function() {
        assertEquals('chrome://history/syncedTabs', window.location.href);

        assertEquals('syncedTabs', app.$.content.selected);
        assertEquals(app.$$('#synced-devices'), app.$.content.selectedItem);
      });
    });

    test('routing to /journeys may change active view', function() {
      assertEquals('history', app.$.content.selected);
      assertEquals(app.$$('#history'), app.$['tabs-content'].selectedItem);

      navigateTo('/journeys');
      return flushTasks().then(function() {
        assertEquals('chrome://history/journeys', window.location.href);

        assertEquals('history', app.$.content.selected);
        assertEquals(!!app.$$('#history-clusters'), isHistoryClustersEnabled);
        assertEquals(
            isHistoryClustersEnabled ? app.$$('#history-clusters') :
                                       app.$$('#history'),
            app.$['tabs-content'].selectedItem);
      });
    });

    test('routing to /journeys may update sidebar menu item', function() {
      assertEquals('chrome://history/', sidebar.$.history.href);
      assertEquals('history', sidebar.$.history.path);

      navigateTo('/journeys');
      return flushTasks().then(function() {
        // Currently selected history view is preserved in sidebar menu item.
        assertEquals(
            isHistoryClustersEnabled ? 'chrome://history/journeys' :
                                       'chrome://history/',
            sidebar.$.history.href);
        assertEquals(
            isHistoryClustersEnabled ? 'journeys' : 'history',
            sidebar.$.history.path);
      });
    });

    test('route updates from tabs and sidebar menu items', function() {
      assertEquals('history', sidebar.$.menu.selected);
      assertEquals('chrome://history/', window.location.href);

      sidebar.$.syncedTabs.click();
      assertEquals('syncedTabs', sidebar.$.menu.selected);
      assertEquals('chrome://history/syncedTabs', window.location.href);

      // Currently selected history view is preserved in sidebar menu item.
      keyDownOn(sidebar.$.history, 0, '', ' ');
      assertEquals('history', sidebar.$.menu.selected);
      assertEquals('chrome://history/', window.location.href);

      const historyTabs = app.$$('cr-tabs');
      assertEquals(!!historyTabs, isHistoryClustersEnabled);

      if (isHistoryClustersEnabled) {
        historyTabs.selected = 1;
        assertEquals('journeys', sidebar.$.menu.selected);
        assertEquals('chrome://history/journeys', window.location.href);

        keyDownOn(sidebar.$.syncedTabs, 0, '', ' ');
        assertEquals('syncedTabs', sidebar.$.menu.selected);
        assertEquals('chrome://history/syncedTabs', window.location.href);

        // Currently selected history view is preserved in sidebar menu item.
        keyDownOn(sidebar.$.history, 0, '', ' ');
        assertEquals('journeys', sidebar.$.menu.selected);
        assertEquals('chrome://history/journeys', window.location.href);

        historyTabs.selected = 0;
        assertEquals('history', sidebar.$.menu.selected);
        assertEquals('chrome://history/', window.location.href);
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

    test('search is preserved across tabs and sidebar menu items', function() {
      const searchTerm = 'Soldier76';
      assertEquals('history', sidebar.$.menu.selected);
      navigateTo('/?q=' + searchTerm);

      sidebar.$.syncedTabs.click();
      assertEquals('syncedTabs', sidebar.$.menu.selected);
      assertEquals(searchTerm, toolbar.searchTerm);
      assertEquals(
          'chrome://history/syncedTabs?q=' + searchTerm, window.location.href);

      sidebar.$.history.click();
      assertEquals('history', sidebar.$.menu.selected);
      assertEquals(searchTerm, toolbar.searchTerm);
      assertEquals('chrome://history/?q=' + searchTerm, window.location.href);

      if (isHistoryClustersEnabled) {
        app.$$('cr-tabs').selected = 1;
        assertEquals('journeys', sidebar.$.menu.selected);
        assertEquals(searchTerm, toolbar.searchTerm);
        assertEquals(
            'chrome://history/journeys?q=' + searchTerm, window.location.href);
      }
    });
  });
});
