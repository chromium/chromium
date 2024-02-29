// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';
import 'chrome://history/lazy_load.js';

import type {HistoryAppElement, HistoryEntry} from 'chrome://history/history.js';
import {BrowserServiceImpl, ensureLazyLoaded, HistoryPageViewHistogram, SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram} from 'chrome://history/history.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry, createHistoryInfo, createSession, createWindow, disableLinkClicks, navigateTo} from './test_util.js';

suite('Metrics', function() {
  let testService: TestBrowserService;
  let app: HistoryAppElement;
  let histogramMap: {[key: string]: {[key: string]: number}};
  let actionMap: {[key: string]: number};

  suiteSetup(function() {
    disableLinkClicks();
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);

    actionMap = testService.actionMap;
    histogramMap = testService.histogramMap;

    app = document.createElement('history-app');
  });

  /**
   * @param queryResults The query results to initialize the page with.
   * @param query The query to use in the QueryInfo.
   * @return Promise that resolves when initialization is complete.
   */
  function finishSetup(
      queryResults: HistoryEntry[], query?: string): Promise<void> {
    testService.setQueryResult(
        {info: createHistoryInfo(query), value: queryResults});
    document.body.appendChild(app);
    return Promise
        .all([
          testService.whenCalled('queryHistory'),
          ensureLazyLoaded(),
        ])
        .then(function() {
          webUIListenerCallback('sign-in-state-changed', false);
          return flushTasks();
        });
  }

  test('History.HistoryPageView', async () => {
    await finishSetup([]);

    const histogram = histogramMap['History.HistoryPageView'];
    assertTrue(!!histogram);
    assertEquals(1, histogram[HistoryPageViewHistogram.HISTORY]);

    navigateTo('/syncedTabs', app);
    assertEquals(1, histogram[HistoryPageViewHistogram.SIGNIN_PROMO]);
    await testService.whenCalled('otherDevicesInitialized');

    testService.resetResolver('recordHistogram');
    webUIListenerCallback('sign-in-state-changed', true);
    await testService.whenCalled('recordHistogram');

    assertEquals(1, histogram[HistoryPageViewHistogram.SYNCED_TABS]);
    navigateTo('/history', app);
    assertEquals(2, histogram[HistoryPageViewHistogram.HISTORY]);
  });

  test('history-list', async () => {
    // Create a history entry that is between 7 and 8 days in the past. For the
    // purposes of the tested functionality, we consider a day to be a 24 hour
    // period, with no regard to DST shifts.
    const weekAgo =
        new Date(new Date().getTime() - 7 * 24 * 60 * 60 * 1000 - 1);

    const historyEntry =
        createHistoryEntry(weekAgo.getTime(), 'http://www.google.com');
    historyEntry.starred = true;
    await finishSetup([
      createHistoryEntry(weekAgo.getTime(), 'http://www.example.com'),
      historyEntry,
    ]);
    await flushTasks();

    let items = app.$.history.shadowRoot!.querySelectorAll('history-item');
    assertTrue(!!items[1]);
    items[1].shadowRoot!.querySelector<HTMLElement>('#bookmark-star')!.click();
    assertEquals(1, actionMap['BookmarkStarClicked']);
    items[1].$.link.click();
    assertEquals(1, actionMap['EntryLinkClick']);

    testService.resetResolver('queryHistory');
    testService.setQueryResult({
      info: createHistoryInfo('goog'),
      value: [
        createHistoryEntry(weekAgo.getTime(), 'http://www.google.com'),
        createHistoryEntry(weekAgo.getTime(), 'http://www.google.com'),
        createHistoryEntry(weekAgo.getTime(), 'http://www.google.com'),
      ],
    });
    app.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'goog'}}));
    assertEquals(1, actionMap['Search']);
    app.set('queryState_.incremental', true);
    await Promise.all([
      testService.whenCalled('queryHistory'),
      flushTasks(),
    ]);

    app.$.history.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
    await waitAfterNextRender(app.$.history);
    flush();

    items = app.$.history.shadowRoot!.querySelectorAll('history-item');
    assertTrue(!!items[0]);
    assertTrue(!!items[4]);
    items[0].$.link.click();
    assertEquals(1, actionMap['SearchResultClick']);
    items[0].$.checkbox.click();
    items[4].$.checkbox.click();
    await flushTasks();

    app.$.toolbar.deleteSelectedItems();
    assertEquals(1, actionMap['RemoveSelected']);
    await flushTasks();

    app.$.history.shadowRoot!.querySelector<HTMLElement>(
                                 '.cancel-button')!.click();
    assertEquals(1, actionMap['CancelRemoveSelected']);
    app.$.toolbar.deleteSelectedItems();
    await flushTasks();

    app.$.history.shadowRoot!.querySelector<HTMLElement>(
                                 '.action-button')!.click();
    assertEquals(1, actionMap['ConfirmRemoveSelected']);
    await flushTasks();

    items = app.$.history.shadowRoot!.querySelectorAll('history-item');
    assertTrue(!!items[0]);
    items[0].$['menu-button'].click();
    await flushTasks();

    app.$.history.shadowRoot!.querySelector<HTMLElement>(
                                 '#menuRemoveButton')!.click();
    await Promise.all([
      testService.whenCalled('removeVisits'),
      flushTasks(),
    ]);
  });

  test('synced-device-manager', async () => {
    const sessionList = [
      createSession(
          'Nexus 5',
          [createWindow(['http://www.google.com', 'http://example.com'])]),
      createSession(
          'Nexus 6',
          [
            createWindow(['http://test.com']),
            createWindow(['http://www.gmail.com', 'http://badssl.com']),
          ]),
    ];
    testService.setForeignSessions(sessionList);
    await finishSetup([]);

    navigateTo('/syncedTabs', app);
    await flushTasks();

    const histogram = histogramMap[SYNCED_TABS_HISTOGRAM_NAME];
    assertTrue(!!histogram);
    assertEquals(1, histogram[SyncedTabsHistogram.INITIALIZED]);

    await testService.whenCalled('getForeignSessions');
    await flushTasks();

    assertEquals(1, histogram[SyncedTabsHistogram.HAS_FOREIGN_DATA]);
    await flushTasks();

    const syncedDeviceManager =
        app.shadowRoot!.querySelector('history-synced-device-manager');
    assertTrue(!!syncedDeviceManager);

    const cards = syncedDeviceManager.shadowRoot!.querySelectorAll(
        'history-synced-device-card');
    assertTrue(!!cards[0]);
    cards[0].$['card-heading'].click();
    assertEquals(1, histogram[SyncedTabsHistogram.COLLAPSE_SESSION]);
    cards[0].$['card-heading'].click();
    assertEquals(1, histogram[SyncedTabsHistogram.EXPAND_SESSION]);
    cards[0].shadowRoot!.querySelectorAll<HTMLElement>(
                            '.website-link')[0]!.click();
    assertEquals(1, histogram[SyncedTabsHistogram.LINK_CLICKED]);

    const menuButton = cards[0].$['menu-button'];
    menuButton.click();
    await flushTasks();

    syncedDeviceManager.shadowRoot!
        .querySelector<HTMLElement>('#menuOpenButton')!.click();
    assertEquals(1, histogram[SyncedTabsHistogram.OPEN_ALL]);

    menuButton.click();
    await flushTasks();

    syncedDeviceManager!.shadowRoot!
        .querySelector<HTMLElement>('#menuDeleteButton')!.click();
    assertEquals(1, histogram[SyncedTabsHistogram.HIDE_FOR_NOW]);
  });

  test('history-clusters-duration', async () => {
    await finishSetup([]);

    navigateTo('/grouped', app);
    await flushTasks();

    navigateTo('/history', app);
    await flushTasks();

    const args = await testService.whenCalled('recordLongTime');
    assertEquals(args[0], 'History.Clusters.WebUISessionDuration');
  });
});
