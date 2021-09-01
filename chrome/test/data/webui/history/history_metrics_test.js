// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService, ensureLazyLoaded, HistoryPageViewHistogram, SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram} from 'chrome://history/history.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {createHistoryEntry, createHistoryInfo, createSession, createWindow, disableLinkClicks, polymerSelectAll} from 'chrome://test/history/test_util.js';
import {flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';

suite('Metrics', function() {
  let testService;
  let app;
  let histogramMap;
  let actionMap;

  suiteSetup(function() {
    disableLinkClicks();
  });

  setup(async () => {
    document.body.innerHTML = '';

    BrowserService.setInstance(new TestBrowserService());
    testService = BrowserService.getInstance();

    actionMap = testService.actionMap;
    histogramMap = testService.histogramMap;

    app = document.createElement('history-app');
  });

  /**
   * @param {!Array<!HistoryEntry>} queryResults The query results to initialize
   *     the page with.
   * @param {string=} query The query to use in the QueryInfo.
   * @return {!Promise} Promise that resolves when initialization is complete.
   */
  function finishSetup(queryResults, query) {
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
    app.grouped_ = true;

    const histogram = histogramMap['History.HistoryPageView'];
    assertEquals(1, histogram[HistoryPageViewHistogram.HISTORY]);

    app.selectedPage_ = 'syncedTabs';
    assertEquals(1, histogram[HistoryPageViewHistogram.SIGNIN_PROMO]);
    await testService.whenCalled('otherDevicesInitialized');

    testService.resetResolver('recordHistogram');
    webUIListenerCallback('sign-in-state-changed', true);
    await testService.whenCalled('recordHistogram');

    assertEquals(1, histogram[HistoryPageViewHistogram.SYNCED_TABS]);
    app.selectedPage_ = 'history';
    assertEquals(2, histogram[HistoryPageViewHistogram.HISTORY]);
  });

  test('history-list', async () => {
    // Create a history entry that is between 7 and 8 days in the past. For the
    // purposes of the tested functionality, we consider a day to be a 24 hour
    // period, with no regard to DST shifts.
    const weekAgo =
        new Date(new Date().getTime() - 7 * 24 * 60 * 60 * 1000 - 1);

    const historyEntry = createHistoryEntry(weekAgo, 'http://www.google.com');
    historyEntry.starred = true;
    await finishSetup(
        [createHistoryEntry(weekAgo, 'http://www.example.com'), historyEntry]);
    await flushTasks();

    let items = polymerSelectAll(app.$.history, 'history-item');
    items[1].$$('#bookmark-star').click();
    assertEquals(1, actionMap['BookmarkStarClicked']);
    items[1].$.link.click();
    assertEquals(1, actionMap['EntryLinkClick']);
    assertEquals(1, histogramMap['HistoryPage.ClickPosition'][1]);
    assertEquals(1, histogramMap['HistoryPage.ClickPositionSubset'][1]);

    // TODO(https://crbug.com/1000573): Log the contents of this histogram
    // for debugging in case the flakiness reoccurs.
    console.log(Object.keys(histogramMap['HistoryPage.ClickAgeInDays']));

    // The "age in days" histogram should record 8 days, since the history
    // entry was created between 7 and 8 days ago and we round the
    // recorded value up.
    assertEquals(1, histogramMap['HistoryPage.ClickAgeInDays'][8]);
    assertEquals(1, histogramMap['HistoryPage.ClickAgeInDaysSubset'][8]);

    testService.resetResolver('queryHistory');
    testService.setQueryResult({
      info: createHistoryInfo('goog'),
      value: [
        createHistoryEntry(weekAgo, 'http://www.google.com'),
        createHistoryEntry(weekAgo, 'http://www.google.com'),
        createHistoryEntry(weekAgo, 'http://www.google.com'),
      ],
    });
    app.fire('change-query', {search: 'goog'});
    assertEquals(1, actionMap['Search']);
    app.set('queryState_.incremental', true);
    await Promise.all([
      testService.whenCalled('queryHistory'),
      flushTasks(),
    ]);

    app.$.history.$$('iron-list').fire('iron-resize');
    await waitAfterNextRender(app.$.history);
    flush();

    items = polymerSelectAll(app.$.history, 'history-item');
    items[0].$.link.click();
    assertEquals(1, actionMap['SearchResultClick']);
    assertEquals(1, histogramMap['HistoryPage.ClickPosition'][0]);
    assertEquals(1, histogramMap['HistoryPage.ClickPositionSubset'][0]);
    items[0].$.checkbox.click();
    items[4].$.checkbox.click();
    await flushTasks();

    app.$.toolbar.deleteSelectedItems();
    assertEquals(1, actionMap['RemoveSelected']);
    await flushTasks();

    app.$.history.$$('.cancel-button').click();
    assertEquals(1, actionMap['CancelRemoveSelected']);
    app.$.toolbar.deleteSelectedItems();
    await flushTasks();

    app.$.history.$$('.action-button').click();
    assertEquals(1, actionMap['ConfirmRemoveSelected']);
    await flushTasks();

    items = polymerSelectAll(app.$.history, 'history-item');
    items[0].$['menu-button'].click();
    await flushTasks();

    app.$.history.$$('#menuRemoveButton').click();
    await Promise.all([
      testService.whenCalled('removeVisits'),
      flushTasks(),
    ]);

    assertEquals(1, histogramMap['HistoryPage.RemoveEntryPosition'][0]);
    assertEquals(1, histogramMap['HistoryPage.RemoveEntryPositionSubset'][0]);
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
            createWindow(['http://www.gmail.com', 'http://badssl.com'])
          ]),
    ];
    testService.setForeignSessions(sessionList);
    await finishSetup([]);

    app.selectedPage_ = 'syncedTabs';
    await flushTasks();

    const histogram = histogramMap[SYNCED_TABS_HISTOGRAM_NAME];
    assertEquals(1, histogram[SyncedTabsHistogram.INITIALIZED]);

    await testService.whenCalled('getForeignSessions');
    await flushTasks();

    assertEquals(1, histogram[SyncedTabsHistogram.HAS_FOREIGN_DATA]);
    await flushTasks();

    const cards = polymerSelectAll(
        app.$$('#synced-devices'), 'history-synced-device-card');
    cards[0].$['card-heading'].click();
    assertEquals(1, histogram[SyncedTabsHistogram.COLLAPSE_SESSION]);
    cards[0].$['card-heading'].click();
    assertEquals(1, histogram[SyncedTabsHistogram.EXPAND_SESSION]);
    polymerSelectAll(cards[0], '.website-link')[0].click();
    assertEquals(1, histogram[SyncedTabsHistogram.LINK_CLICKED]);

    const menuButton = cards[0].$['menu-button'];
    menuButton.click();
    await flushTasks();

    app.$$('#synced-devices').$$('#menuOpenButton').click();
    assertEquals(1, histogram[SyncedTabsHistogram.OPEN_ALL]);

    menuButton.click();
    await flushTasks();

    app.$$('#synced-devices').$$('#menuDeleteButton').click();
    assertEquals(1, histogram[SyncedTabsHistogram.HIDE_FOR_NOW]);
  });
});
