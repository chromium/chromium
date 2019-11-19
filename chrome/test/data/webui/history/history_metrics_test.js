// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {history.BrowserService}
 */
const TestMetricsBrowserService = function() {
  this.histogramMap = {};
  this.actionMap = {};
};

suite('Metrics', function() {
  let service;
  let app;
  let histogramMap;
  let actionMap;

  suiteSetup(function() {
    disableLinkClicks();

    TestMetricsBrowserService.prototype = {
      __proto__: history.BrowserService.prototype,

      /** @override */
      recordHistogram: function(histogram, value, max) {
        assertTrue(value < max);

        if (!(histogram in this.histogramMap)) {
          this.histogramMap[histogram] = {};
        }

        if (!(value in this.histogramMap[histogram])) {
          this.histogramMap[histogram][value] = 0;
        }

        this.histogramMap[histogram][value]++;
      },

      /** @override */
      recordAction: function(action) {
        if (!(action in this.actionMap)) {
          this.actionMap[action] = 0;
        }

        this.actionMap[action]++;
      },

      /** @override */
      deleteItems: function() {
        return test_util.flushTasks();
      }
    };
  });

  setup(function() {
    history.BrowserService.instance_ = new TestMetricsBrowserService();
    service = history.BrowserService.getInstance();

    actionMap = service.actionMap;
    histogramMap = service.histogramMap;

    app = replaceApp();
    updateSignInState(false);
    return test_util.flushTasks();
  });

  test('History.HistoryPageView', function() {
    app.grouped_ = true;

    const histogram = histogramMap['History.HistoryPageView'];
    assertEquals(1, histogram[HistoryPageViewHistogram.HISTORY]);

    app.selectedPage_ = 'syncedTabs';
    assertEquals(1, histogram[HistoryPageViewHistogram.SIGNIN_PROMO]);
    updateSignInState(true);
    return test_util.flushTasks().then(() => {
      assertEquals(1, histogram[HistoryPageViewHistogram.SYNCED_TABS]);
      app.selectedPage_ = 'history';
      assertEquals(2, histogram[HistoryPageViewHistogram.HISTORY]);
    });
  });

  test('history-list', function() {
    // Create a history entry that is between 7 and 8 days in the past. For the
    // purposes of the tested functionality, we consider a day to be a 24 hour
    // period, with no regard to DST shifts.
    const weekAgo =
        new Date(new Date().getTime() - 7 * 24 * 60 * 60 * 1000 - 1);

    const historyEntry = createHistoryEntry(weekAgo, 'http://www.google.com');
    historyEntry.starred = true;
    app.historyResult(
        createHistoryInfo(),
        [createHistoryEntry(weekAgo, 'http://www.example.com'), historyEntry]);

    return test_util.flushTasks()
        .then(() => {
          const items = polymerSelectAll(app.$.history, 'history-item');
          MockInteractions.tap(items[1].$$('#bookmark-star'));
          assertEquals(1, actionMap['BookmarkStarClicked']);
          MockInteractions.tap(items[1].$.link);
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

          app.fire('change-query', {search: 'goog'});
          assertEquals(1, actionMap['Search']);
          app.set('queryState_.incremental', true);
          app.historyResult(createHistoryInfo('goog'), [
            createHistoryEntry(weekAgo, 'http://www.google.com'),
            createHistoryEntry(weekAgo, 'http://www.google.com'),
            createHistoryEntry(weekAgo, 'http://www.google.com')
          ]);
          return test_util.flushTasks();
        })
        .then(() => {
          items = polymerSelectAll(app.$.history, 'history-item');
          MockInteractions.tap(items[0].$.link);
          assertEquals(1, actionMap['SearchResultClick']);
          assertEquals(1, histogramMap['HistoryPage.ClickPosition'][0]);
          assertEquals(1, histogramMap['HistoryPage.ClickPositionSubset'][0]);
          MockInteractions.tap(items[0].$.checkbox);
          MockInteractions.tap(items[4].$.checkbox);
          return test_util.flushTasks();
        })
        .then(() => {
          app.$.toolbar.deleteSelectedItems();
          assertEquals(1, actionMap['RemoveSelected']);
          return test_util.flushTasks();
        })
        .then(() => {
          MockInteractions.tap(app.$.history.$$('.cancel-button'));
          assertEquals(1, actionMap['CancelRemoveSelected']);
          app.$.toolbar.deleteSelectedItems();
          return test_util.flushTasks();
        })
        .then(() => {
          MockInteractions.tap(app.$.history.$$('.action-button'));
          assertEquals(1, actionMap['ConfirmRemoveSelected']);
          return test_util.flushTasks();
        })
        .then(() => {
          items = polymerSelectAll(app.$.history, 'history-item');
          MockInteractions.tap(items[0].$['menu-button']);
          return test_util.flushTasks();
        })
        .then(() => {
          MockInteractions.tap(app.$.history.$$('#menuRemoveButton'));
          return test_util.flushTasks();
        })
        .then(() => {
          assertEquals(1, histogramMap['HistoryPage.RemoveEntryPosition'][0]);
          assertEquals(
              1, histogramMap['HistoryPage.RemoveEntryPositionSubset'][0]);
        });
  });

  test('synced-device-manager', function() {
    app.selectedPage_ = 'syncedTabs';
    let histogram;
    let menuButton;
    return test_util.flushTasks()
        .then(() => {
          histogram = histogramMap[SYNCED_TABS_HISTOGRAM_NAME];
          assertEquals(1, histogram[SyncedTabsHistogram.INITIALIZED]);

          const sessionList = [
            createSession('Nexus 5', [createWindow([
                            'http://www.google.com', 'http://example.com'
                          ])]),
            createSession(
                'Nexus 6',
                [
                  createWindow(['http://test.com']),
                  createWindow(['http://www.gmail.com', 'http://badssl.com'])
                ]),
          ];
          setForeignSessions(sessionList);
          return test_util.flushTasks();
        })
        .then(() => {
          assertEquals(1, histogram[SyncedTabsHistogram.HAS_FOREIGN_DATA]);
          return test_util.flushTasks();
        })
        .then(() => {
          cards = polymerSelectAll(
              app.$$('#synced-devices'), 'history-synced-device-card');
          MockInteractions.tap(cards[0].$['card-heading']);
          assertEquals(1, histogram[SyncedTabsHistogram.COLLAPSE_SESSION]);
          MockInteractions.tap(cards[0].$['card-heading']);
          assertEquals(1, histogram[SyncedTabsHistogram.EXPAND_SESSION]);
          MockInteractions.tap(polymerSelectAll(cards[0], '.website-link')[0]);
          assertEquals(1, histogram[SyncedTabsHistogram.LINK_CLICKED]);

          menuButton = cards[0].$['menu-button'];
          MockInteractions.tap(menuButton);
          return test_util.flushTasks();
        })
        .then(() => {
          MockInteractions.tap(app.$$('#synced-devices').$$('#menuOpenButton'));
          assertEquals(1, histogram[SyncedTabsHistogram.OPEN_ALL]);

          MockInteractions.tap(menuButton);
          return test_util.flushTasks();
        })
        .then(() => {
          MockInteractions.tap(
              app.$$('#synced-devices').$$('#menuDeleteButton'));
          assertEquals(1, histogram[SyncedTabsHistogram.HIDE_FOR_NOW]);
        });
  });
});
