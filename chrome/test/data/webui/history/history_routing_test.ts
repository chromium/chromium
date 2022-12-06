// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {BrowserProxyImpl, BrowserServiceImpl, HistoryAppElement, HistorySideBarElement, MetricsProxyImpl} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

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

    [true, false].forEach(isHistoryClustersVisibleManagedByPolicy => {
      const suiteSuffix =
          isHistoryClustersVisibleManagedByPolicy ? 'managed' : 'not-managed';
      suite(`toggle-history-clusters-sidebar-menu-item-${suiteSuffix}`, () => {
        suiteSetup(() => {
          loadTimeData.overrideValues({
            isHistoryClustersVisibleManagedByPolicy,
          });
        });

        test('menu item visiblity', () => {
          const visible = isHistoryClustersEnabled &&
              !isHistoryClustersVisibleManagedByPolicy;
          assertEquals(!visible, sidebar.$['toggle-history-clusters'].hidden);
        });
      });
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

    test('routing to /journeys may change active view', function() {
      assertEquals('history', app.$.content.selected);
      assertEquals(
          app.shadowRoot!.querySelector('#history'),
          app.$['tabs-content'].selectedItem);

      navigateTo('/journeys', app);
      return flushTasks().then(function() {
        assertEquals('chrome://history/journeys', window.location.href);

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

    test('routing to /journeys may update sidebar menu item', function() {
      assertEquals('chrome://history/', sidebar.$.history.href);
      assertEquals('history', sidebar.$.history.getAttribute('path'));

      navigateTo('/journeys', app);
      return flushTasks().then(function() {
        // Currently selected history view is preserved in sidebar menu item.
        assertEquals(
            isHistoryClustersEnabled ? 'chrome://history/journeys' :
                                       'chrome://history/',
            sidebar.$.history.href);
        assertEquals(
            isHistoryClustersEnabled ? 'journeys' : 'history',
            sidebar.$.history.getAttribute('path'));
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

      const historyTabs = app.shadowRoot!.querySelector('cr-tabs');
      assertEquals(!!historyTabs, isHistoryClustersEnabled);

      if (isHistoryClustersEnabled) {
        assertTrue(!!historyTabs);
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

    test('toggle history clusters on/off from sidebar menu item', async () => {
      if (!isHistoryClustersEnabled) {
        return;
      }

      const handler = testBrowserProxy.handler;

      assertEquals('history', sidebar.$.menu.selected);
      assertEquals('chrome://history/', window.location.href);

      // Navigate to chrome://history/journeys.
      app.shadowRoot!.querySelector('cr-tabs')!.selected = 1;
      assertEquals('journeys', sidebar.$.menu.selected);
      assertEquals('chrome://history/journeys', window.location.href);
      await flushTasks();
      assertTrue(app.shadowRoot!.querySelector('#history-clusters')!.classList
                     .contains('iron-selected'));

      handler.setResultFor(
          'toggleVisibility', Promise.resolve({visible: false}));

      // Verify the menu item label and press it.
      assertEquals(
          'Disable', sidebar.$['toggle-history-clusters'].textContent!.trim());
      keyDownOn(sidebar.$['toggle-history-clusters'], 0, '', ' ');

      // Verify that the browser is notified and the histogram is recorded.
      let visible =
          await testMetricsProxy.whenCalled('recordToggledVisibility');
      assertFalse(visible);
      visible = await handler.whenCalled('toggleVisibility');
      assertFalse(visible);

      // Toggling history clusters off navigates to chrome://history/.
      assertEquals('history', sidebar.$.menu.selected);
      assertEquals('chrome://history/', window.location.href);
      assertFalse(app.shadowRoot!.querySelector('#history-clusters')!.classList
                      .contains('iron-selected'));

      handler.reset();
      testMetricsProxy.reset();

      handler.setResultFor(
          'toggleVisibility', Promise.resolve({visible: true}));

      // Verify the updated menu item label and press it again.
      assertEquals(
          'Enable', sidebar.$['toggle-history-clusters'].textContent!.trim());
      keyDownOn(sidebar.$['toggle-history-clusters'], 0, '', ' ');

      // Verify that the browser is notified and the histogram is recorded.
      visible = await testMetricsProxy.whenCalled('recordToggledVisibility');
      assertTrue(visible);
      visible = await handler.whenCalled('toggleVisibility');
      assertTrue(visible);

      // Toggling history clusters on navigates to chrome://history/journeys.
      assertEquals('journeys', sidebar.$.menu.selected);
      assertEquals('chrome://history/journeys', window.location.href);
      assertTrue(app.shadowRoot!.querySelector('#history-clusters')!.classList
                     .contains('iron-selected'));
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

    test('search is preserved across tabs and sidebar menu items', function() {
      const searchTerm = 'Soldier76';
      assertEquals('history', sidebar.$.menu.selected);
      navigateTo('/?q=' + searchTerm, app);

      sidebar.$.syncedTabs.click();
      assertEquals('syncedTabs', sidebar.$.menu.selected);
      assertEquals(searchTerm, app.$.toolbar.searchTerm);
      assertEquals(
          'chrome://history/syncedTabs?q=' + searchTerm, window.location.href);

      sidebar.$.history.click();
      assertEquals('history', sidebar.$.menu.selected);
      assertEquals(searchTerm, app.$.toolbar.searchTerm);
      assertEquals('chrome://history/?q=' + searchTerm, window.location.href);

      if (isHistoryClustersEnabled) {
        app.shadowRoot!.querySelector('cr-tabs')!.selected = 1;
        assertEquals('journeys', sidebar.$.menu.selected);
        assertEquals(searchTerm, app.$.toolbar.searchTerm);
        assertEquals(
            'chrome://history/journeys?q=' + searchTerm, window.location.href);
      }
    });
  });
});
