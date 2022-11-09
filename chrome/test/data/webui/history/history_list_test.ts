// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserServiceImpl, CrDialogElement, ensureLazyLoaded, HistoryAppElement, HistoryEntry, HistoryItemElement, HistoryListElement, HistoryToolbarElement} from 'chrome://history/history.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry, createHistoryInfo, shiftClick, waitForEvent} from './test_util.js';

const history_list_test = {
  suiteName: 'HistoryListTest',
  TestNames: {
    DeletingSingleItem: 'deleting single item',
    CancellingSelectionOfMultipleItems:
        'cancelling selection of multiple items',
    SelectionOfMultipleItemsUsingShiftClick:
        'selection of multiple items using shift click',
    DisablingCtrlAOnSyncedTabsPage:
        'disabling ctrl + a command on syncedTabs page',
    SettingFirstAndLastItems: 'setting first and last items',
    UpdatingHistoryResults: 'updating history results',
    DeletingMultipleItemsFromView: 'deleting multiple items from view',
    SearchResultsDisplayWithCorrectItemTitle:
        'search results display with correct item title',
    CorrectDisplayMessageWhenNoHistoryAvailable:
        'correct display message when no history available',
    MoreFromThisSiteSendsAndSetsCorrectData:
        'more from this site sends and sets correct data',
    ScrollingHistoryListCausesToolbarShadowToAppear:
        'scrolling history list causes toolbar shadow to appear',
    ChangingSearchDeselectsItems: 'changing search deselects items',
    DeleteItemsEndToEnd: 'delete items end to end',
    DeleteViaMenuButton: 'delete via menu button',
    DeleteDisabledWhilePending: 'delete disabled while pending',
    DeletingItemsUsingShortcuts: 'deleting items using shortcuts',
    DeleteDialogClosedOnBackNavigation:
        'delete dialog closed on back navigation',
    ClickingFileUrlSendsMessageToChrome:
        'clicking file:// url sends message to chrome',
    DeleteHistoryResultsInQueryHistoryEvent:
        'deleteHistory results in query-history event',
  },
};

Object.assign(window, {history_list_test});

suite(history_list_test.suiteName, function() {
  let app: HistoryAppElement;
  let element: HistoryListElement;
  let toolbar: HistoryToolbarElement;
  let testService: TestBrowserService;

  const TEST_HISTORY_RESULTS = [
    createHistoryEntry('2016-03-15', 'https://www.google.com'),
    createHistoryEntry('2016-03-14 10:00', 'https://www.example.com'),
    createHistoryEntry('2016-03-14 9:00', 'https://www.google.com'),
    createHistoryEntry('2016-03-13', 'https://en.wikipedia.org'),
  ];
  TEST_HISTORY_RESULTS[2]!.starred = true;

  const ADDITIONAL_RESULTS = [
    createHistoryEntry('2016-03-13 10:00', 'https://en.wikipedia.org'),
    createHistoryEntry('2016-03-13 9:50', 'https://www.youtube.com'),
    createHistoryEntry('2016-03-11', 'https://www.google.com'),
    createHistoryEntry('2016-03-10', 'https://www.example.com'),
  ];

  setup(function() {
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);

    app = document.createElement('history-app');
  });

  /**
   * @param queryResults The query results to initialize
   *     the page with.
   * @param query The query to use in the QueryInfo.
   * @return Promise that resolves when initialization is complete
   *     and the lazy loaded module has been loaded.
   */
  function finishSetup(
      queryResults: HistoryEntry[], query?: string): Promise<any> {
    testService.setQueryResult(
        {info: createHistoryInfo(query), value: queryResults});
    document.body.appendChild(app);

    element = app.$.history;
    toolbar = app.$.toolbar;
    app.shadowRoot!.querySelector(
                       'history-query-manager')!.queryState.incremental = true;
    return Promise.all([
      testService.whenCalled('queryHistory'),
      ensureLazyLoaded(),
    ]);
  }

  function getHistoryData(): HistoryEntry[] {
    return element.$['infinite-list'].items! as HistoryEntry[];
  }

  test(history_list_test.TestNames.DeletingSingleItem, function() {
    return finishSetup([createHistoryEntry('2015-01-01', 'http://example.com')])
        .then(flushTasks)
        .then(function() {
          assertEquals(getHistoryData().length, 1);
          flush();
          const items = element.shadowRoot!.querySelectorAll('history-item');

          assertEquals(1, items.length);
          items[0]!.$.checkbox.click();
          assertDeepEquals([true], getHistoryData().map(i => i.selected));
          return flushTasks();
        })
        .then(function() {
          toolbar.deleteSelectedItems();
          return flushTasks();
        })
        .then(function() {
          const dialog = element.$.dialog.get();
          assertTrue(dialog.open);
          testService.resetResolver('queryHistory');
          element.shadowRoot!.querySelector<HTMLElement>(
                                 '.action-button')!.click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(visits) {
          assertEquals(1, visits.length);
          assertEquals('http://example.com', visits[0].url);
          assertEquals(Date.parse('2015-01-01 UTC'), visits[0].timestamps[0]);

          // The list should fire a query-history event which results in a
          // queryHistory call, since deleting the only item results in an
          // empty history list.
          return testService.whenCalled('queryHistory');
        });
  });

  test(
      history_list_test.TestNames.CancellingSelectionOfMultipleItems,
      function() {
        return finishSetup(TEST_HISTORY_RESULTS)
            .then(flushTasks)
            .then(function() {
              element.shadowRoot!.querySelector('iron-list')!.fire(
                  'iron-resize');
              return waitAfterNextRender(element);
            })
            .then(function() {
              flush();
              const items =
                  element.shadowRoot!.querySelectorAll('history-item');

              items[2]!.$.checkbox.click();
              items[3]!.$.checkbox.click();

              // Make sure that the array of data that determines whether or not
              // an item is selected is what we expect after selecting the two
              // items.
              assertDeepEquals(
                  [false, false, true, true],
                  getHistoryData().map(i => i.selected));

              toolbar.clearSelectedItems();

              // Make sure that clearing the selection updates both the array
              // and the actual history-items affected.
              assertDeepEquals(
                  [false, false, false, false],
                  getHistoryData().map(i => i.selected));

              assertFalse(items[2]!.selected);
              assertFalse(items[3]!.selected);
            });
      });

  test(
      history_list_test.TestNames.SelectionOfMultipleItemsUsingShiftClick,
      function() {
        return finishSetup(TEST_HISTORY_RESULTS)
            .then(flushTasks)
            .then(function() {
              element.shadowRoot!.querySelector('iron-list')!.fire(
                  'iron-resize');
              return waitAfterNextRender(element);
            })
            .then(function() {
              flush();
              const items =
                  element.shadowRoot!.querySelectorAll('history-item');

              items[1]!.$.checkbox.click();
              assertDeepEquals(
                  [false, true, false, false],
                  getHistoryData().map(i => i.selected));
              assertDeepEquals([1], Array.from(element.selectedItems).sort());

              // Shift-select to the last item.
              shiftClick(items[3]!.$.checkbox);
              assertDeepEquals(
                  [false, true, true, true],
                  getHistoryData().map(i => i.selected));
              assertDeepEquals(
                  [1, 2, 3], Array.from(element.selectedItems).sort());

              // Shift-select back to the first item.
              shiftClick(items[0]!.$.checkbox);
              assertDeepEquals(
                  [true, true, true, true],
                  getHistoryData().map(i => i.selected));
              assertDeepEquals(
                  [0, 1, 2, 3], Array.from(element.selectedItems).sort());

              // Shift-deselect to the third item.
              shiftClick(items[2]!.$.checkbox);
              assertDeepEquals(
                  [false, false, false, true],
                  getHistoryData().map(i => i.selected));
              assertDeepEquals([3], Array.from(element.selectedItems).sort());

              // Select the second item.
              items[1]!.$.checkbox.click();
              assertDeepEquals(
                  [false, true, false, true],
                  getHistoryData().map(i => i.selected));
              assertDeepEquals(
                  [1, 3], Array.from(element.selectedItems).sort());

              // Shift-deselect to the last item.
              shiftClick(items[3]!.$.checkbox);
              assertDeepEquals(
                  [false, false, false, false],
                  getHistoryData().map(i => i.selected));
              assertDeepEquals([], Array.from(element.selectedItems).sort());

              // Shift-select back to the third item.
              shiftClick(items[2]!.$.checkbox);
              assertDeepEquals(
                  [false, false, true, true],
                  getHistoryData().map(i => i.selected));
              assertDeepEquals(
                  [2, 3], Array.from(element.selectedItems).sort());

              // Remove selected items.
              element.removeItemsForTest(Array.from(element.selectedItems));
              assertDeepEquals(
                  ['https://www.google.com', 'https://www.example.com'],
                  getHistoryData().map(i => i.title));
            });
      });

  // See http://crbug.com/845802.
  test(history_list_test.TestNames.DisablingCtrlAOnSyncedTabsPage, function() {
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(function() {
          app.shadowRoot!.querySelector('history-router')!.selectedPage =
              'syncedTabs';
          return flushTasks();
        })
        .then(function() {
          const field = toolbar.$.mainToolbar.getSearchField();
          field.blur();
          assertFalse(field.showingSearch);

          const modifier = isMac ? 'meta' : 'ctrl';
          pressAndReleaseKeyOn(document.body, 65, modifier, 'a');

          assertDeepEquals(
              [false, false, false, false],
              getHistoryData().map(i => i.selected));
        });
  });

  test(history_list_test.TestNames.SettingFirstAndLastItems, function() {
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(flushTasks)
        .then(function() {
          element.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          const items = element.shadowRoot!.querySelectorAll('history-item');
          assertTrue(items[0]!.isCardStart);
          assertTrue(items[0]!.isCardEnd);
          assertFalse(items[1]!.isCardEnd);
          assertFalse(items[2]!.isCardStart);
          assertTrue(items[2]!.isCardEnd);
          assertTrue(items[3]!.isCardStart);
          assertTrue(items[3]!.isCardEnd);
        });
  });

  function loadWithAdditionalResults() {
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(function() {
          testService.resetResolver('queryHistory');
          testService.setQueryResult(
              {info: createHistoryInfo(), value: ADDITIONAL_RESULTS});
          element.dispatchEvent(new CustomEvent(
              'query-history', {detail: true, bubbles: true, composed: true}));
          return testService.whenCalled('queryHistoryContinuation');
        })
        .then(flushTasks);
  }

  test(history_list_test.TestNames.UpdatingHistoryResults, function() {
    return loadWithAdditionalResults()
        .then(function() {
          element.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          const items = element.shadowRoot!.querySelectorAll('history-item');
          assertTrue(items[3]!.isCardStart);
          assertTrue(items[5]!.isCardEnd);

          assertTrue(items[6]!.isCardStart);
          assertTrue(items[6]!.isCardEnd);

          assertTrue(items[7]!.isCardStart);
          assertTrue(items[7]!.isCardEnd);
        });
  });

  test(history_list_test.TestNames.DeletingMultipleItemsFromView, function() {
    return loadWithAdditionalResults()
        .then(function() {
          element.removeItemsForTest([2, 5, 7]);
          return flushTasks();
        })
        .then(function() {
          element.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          const items = element.shadowRoot!.querySelectorAll('history-item');

          const historyData = getHistoryData();
          assertEquals(historyData.length, 5);
          assertEquals(historyData[0]!.dateRelativeDay, '2016-03-15');
          assertEquals(historyData[2]!.dateRelativeDay, '2016-03-13');
          assertEquals(historyData[4]!.dateRelativeDay, '2016-03-11');

          // Checks that the first and last items have been reset correctly.
          assertTrue(items[2]!.isCardStart);
          assertTrue(items[3]!.isCardEnd);
          assertTrue(items[4]!.isCardStart);
          assertTrue(items[4]!.isCardEnd);
        });
  });

  test(
      history_list_test.TestNames.SearchResultsDisplayWithCorrectItemTitle,
      function() {
        return finishSetup(
                   [createHistoryEntry('2016-03-15', 'https://www.google.com')])
            .then(function() {
              element.searchedTerm = 'Google';
              flushTasks();
            })
            .then(function() {
              flush();
              const item = element.shadowRoot!.querySelector('history-item')!;
              assertTrue(item.isCardStart);
              const heading =
                  item.shadowRoot!.querySelector<HTMLElement>(
                                      '#date-accessed')!.textContent!;
              const title = item.$.link;

              // Check that the card title displays the search term somewhere.
              const index = heading.indexOf('Google');
              assertTrue(index !== -1);

              // Check that the search term is bolded correctly in the
              // history-item.
              assertGT(
                  title.children[1]!.innerHTML!.indexOf('<b>google</b>'), -1);
            });
      });

  test(
      history_list_test.TestNames.CorrectDisplayMessageWhenNoHistoryAvailable,
      function() {
        return finishSetup([])
            .then(flushTasks)
            .then(function() {
              assertFalse(element.$['no-results'].hidden);
              assertNotEquals('', element.$['no-results'].textContent!.trim());
              assertTrue(element.$['infinite-list'].hidden);

              testService.setQueryResult(
                  {info: createHistoryInfo(), value: TEST_HISTORY_RESULTS});
              element.dispatchEvent(new CustomEvent(
                  'query-history',
                  {bubbles: true, composed: true, detail: false}));
              return testService.whenCalled('queryHistory');
            })
            .then(flushTasks)
            .then(function() {
              assertTrue(element.$['no-results'].hidden);
              assertFalse(element.$['infinite-list'].hidden);
            });
      });

  test(
      history_list_test.TestNames.MoreFromThisSiteSendsAndSetsCorrectData,
      function() {
        let items: NodeListOf<HistoryItemElement>;
        return finishSetup(TEST_HISTORY_RESULTS)
            .then(flushTasks)
            .then(function() {
              element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
                  new CustomEvent(
                      'iron-resize', {bubbles: true, composed: true}));
              return waitAfterNextRender(element);
            })
            .then(function() {
              flush();
              testService.resetResolver('queryHistory');
              testService.setQueryResult({
                info: createHistoryInfo('www.google.com'),
                value: TEST_HISTORY_RESULTS,
              });
              items = element.shadowRoot!.querySelectorAll('history-item');
              items[0]!.$['menu-button'].click();
              element.$.sharedMenu.get();
              element.shadowRoot!.querySelector<HTMLElement>(
                                     '#menuMoreButton')!.click();
              return testService.whenCalled('queryHistory');
            })
            .then(function(query) {
              assertEquals('host:www.google.com', query);
              return flushTasks();
            })
            .then(function() {
              assertEquals(
                  'host:www.google.com',
                  toolbar.$.mainToolbar.getSearchField().getValue());

              element.$.sharedMenu.get().close();
              items[0]!.$['menu-button'].click();
              assertTrue(
                  element.shadowRoot!
                      .querySelector<HTMLElement>('#menuMoreButton')!.hidden);

              element.$.sharedMenu.get().close();
              items[1]!.$['menu-button'].click();
              assertFalse(
                  element.shadowRoot!
                      .querySelector<HTMLElement>('#menuMoreButton')!.hidden);
            });
      });

  // TODO(calamity): Reenable this test after fixing flakiness.
  // See http://crbug.com/640862.
  test.skip(
      history_list_test.TestNames
          .ScrollingHistoryListCausesToolbarShadowToAppear,
      () => {
        function loadMoreResults(numReloads: number): Promise<any> {
          testService.resetResolver('queryHistory');
          testService.setQueryResult(
              {info: createHistoryInfo(), value: TEST_HISTORY_RESULTS});
          element.dispatchEvent(new CustomEvent(
              'query-history', {bubbles: true, composed: true, detail: true}));
          const promise = testService.whenCalled('queryHistoryContinuation');
          return numReloads === 1 ? promise : promise.then(() => {
            return loadMoreResults(numReloads - 1);
          });
        }
        return finishSetup(TEST_HISTORY_RESULTS)
            .then(() => {
              return loadMoreResults(9);
            })
            .then(flushTasks)
            .then(function() {
              element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
                  new CustomEvent(
                      'iron-resize', {bubbles: true, composed: true}));
              return waitAfterNextRender(element);
            })
            .then(() => {
              assertFalse(app.hasAttribute('toolbar-shadow_'));
              element.$['infinite-list'].scrollToIndex(20);
              return waitForEvent(app, 'toolbar-shadow_-changed');
            })
            .then(() => {
              assertTrue(app.hasAttribute('toolbar-shadow_'));
              element.$['infinite-list'].scrollToIndex(0);
              return waitForEvent(app, 'toolbar-shadow_-changed');
            })
            .then(() => {
              assertFalse(app.hasAttribute('toolbar-shadow_'));
            });
      });

  test(history_list_test.TestNames.ChangingSearchDeselectsItems, function() {
    return finishSetup(
               [createHistoryEntry('2016-06-9', 'https://www.example.com')],
               'ex')
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          flush();
          const item = element.shadowRoot!.querySelector('history-item')!;
          item.$.checkbox.click();

          assertEquals(1, toolbar.count);
          app.shadowRoot!.querySelector(
                             'history-query-manager')!.queryState.incremental =
              false;

          testService.resetResolver('queryHistory');
          testService.setQueryResult({
            info: createHistoryInfo('ample'),
            value: [createHistoryEntry('2016-06-9', 'https://www.example.com')],
          });
          element.dispatchEvent(new CustomEvent(
              'query-history', {bubbles: true, composed: true, detail: false}));
          return testService.whenCalled('queryHistory');
        })
        .then(function() {
          assertEquals(0, toolbar.count);
        });
  });

  test(history_list_test.TestNames.DeleteItemsEndToEnd, function() {
    let dialog: CrDialogElement;
    return loadWithAdditionalResults()
        .then(function() {
          element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
              new CustomEvent('iron-resize', {bubbles: true, composed: true}));
          return waitAfterNextRender(element);
        })
        .then(function() {
          dialog = element.$.dialog.get();
          return flushTasks();
        })
        .then(function() {
          flush();
          const items = element.shadowRoot!.querySelectorAll('history-item');

          items[2]!.$.checkbox.click();
          items[5]!.$.checkbox.click();
          items[7]!.$.checkbox.click();

          return flushTasks();
        })
        .then(function() {
          toolbar.deleteSelectedItems();
          return flushTasks();
        })
        .then(function() {
          testService.resetResolver('removeVisits');
          // Confirmation dialog should appear.
          assertTrue(dialog.open);
          element.shadowRoot!.querySelector<HTMLElement>(
                                 '.action-button')!.click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(visits) {
          assertEquals(3, visits.length);
          assertEquals(TEST_HISTORY_RESULTS[2]!.url, visits[0]!.url);
          assertEquals(
              TEST_HISTORY_RESULTS[2]!.allTimestamps[0],
              visits[0]!.timestamps[0]);
          assertEquals(ADDITIONAL_RESULTS[1]!.url, visits[1]!.url);
          assertEquals(
              ADDITIONAL_RESULTS[1]!.allTimestamps[0],
              visits[1]!.timestamps[0]);
          assertEquals(ADDITIONAL_RESULTS[3]!.url, visits[2]!.url);
          assertEquals(
              ADDITIONAL_RESULTS[3]!.allTimestamps[0],
              visits[2]!.timestamps[0]);
          return flushTasks();
        })
        .then(flushTasks)
        .then(function() {
          const historyData = getHistoryData();
          assertEquals(5, historyData.length);
          assertEquals(historyData[0]!.dateRelativeDay, '2016-03-15');
          assertEquals(historyData[2]!.dateRelativeDay, '2016-03-13');
          assertEquals(historyData[4]!.dateRelativeDay, '2016-03-11');
          assertFalse(dialog.open);

          flush();
          // Ensure the UI is correctly updated.
          const items = element.shadowRoot!.querySelectorAll('history-item');

          assertEquals('https://www.google.com', items[0]!.item.title);
          assertEquals('https://www.example.com', items[1]!.item.title);
          assertEquals('https://en.wikipedia.org', items[2]!.item.title);
          assertEquals('https://en.wikipedia.org', items[3]!.item.title);
          assertEquals('https://www.google.com', items[4]!.item.title);
        });
  });

  test(history_list_test.TestNames.DeleteViaMenuButton, function() {
    let items: NodeListOf<HistoryItemElement>;
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(flushTasks)
        .then(function() {
          element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
              new CustomEvent('iron-resize', {bubbles: true, composed: true}));
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          items = element.shadowRoot!.querySelectorAll('history-item');
          items[1]!.$.checkbox.click();
          items[3]!.$.checkbox.click();
          items[1]!.$['menu-button'].click();
          element.$.sharedMenu.get();
          element.shadowRoot!.querySelector<HTMLElement>(
                                 '#menuRemoveButton')!.click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(visits) {
          assertEquals(1, visits.length);
          assertEquals(TEST_HISTORY_RESULTS[1]!.url, visits[0]!.url);
          assertEquals(
              TEST_HISTORY_RESULTS[1]!.allTimestamps[0],
              visits[0]!.timestamps[0]);
          return flushTasks();
        })
        .then(flushTasks)
        .then(function() {
          assertDeepEquals(
              [
                'https://www.google.com',
                'https://www.google.com',
                'https://en.wikipedia.org',
              ],
              getHistoryData().map(item => item.title));

          // Deletion should deselect all.
          assertDeepEquals(
              [false, false, false],
              Array.from(items).slice(0, 3).map(i => i.selected));
        });
  });

  test(history_list_test.TestNames.DeleteDisabledWhilePending, function() {
    let items: NodeListOf<HistoryItemElement>;
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(function() {
          testService.delayDelete();
          return flushTasks();
        })
        .then(function() {
          element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
              new CustomEvent('iron-resize', {bubbles: true, composed: true}));
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          items = element.shadowRoot!.querySelectorAll('history-item');
          items[1]!.$.checkbox.click();
          items[2]!.$.checkbox.click();
          items[1]!.$['menu-button'].click();
          element.$.sharedMenu.get();
          element.shadowRoot!.querySelector<HTMLElement>(
                                 '#menuRemoveButton')!.click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(visits) {
          assertEquals(1, visits.length);
          assertEquals(TEST_HISTORY_RESULTS[1]!.url, visits[0]!.url);
          assertEquals(
              TEST_HISTORY_RESULTS[1]!.allTimestamps[0],
              visits[0]!.timestamps[0]);

          // Deletion is still happening. Verify that menu button and toolbar
          // are disabled.
          assertTrue(element.shadowRoot!
                         .querySelector<HTMLButtonElement>(
                             '#menuRemoveButton')!.disabled);
          assertEquals(2, toolbar.count);
          assertTrue(
              toolbar.shadowRoot!.querySelector('cr-toolbar-selection-overlay')!
                  .deleteDisabled);

          // Key event should be ignored.
          assertEquals(1, testService.getCallCount('removeVisits'));
          pressAndReleaseKeyOn(document.body, 46, '', 'Delete');

          return flushTasks();
        })
        .then(function() {
          assertEquals(1, testService.getCallCount('removeVisits'));
          testService.finishRemoveVisits();
          return flushTasks();
        })
        .then(flushTasks)
        .then(function() {
          // Reselect some items.
          items = element.shadowRoot!.querySelectorAll('history-item');
          items[1]!.$.checkbox.click();
          items[2]!.$.checkbox.click();

          // Check that delete option is re-enabled.
          assertEquals(2, toolbar.count);
          assertFalse(
              toolbar.shadowRoot!.querySelector('cr-toolbar-selection-overlay')!
                  .deleteDisabled);

          // Menu button should also be re-enabled.
          items[1]!.$['menu-button'].click();
          element.$.sharedMenu.get();
          assertFalse(element.shadowRoot!
                          .querySelector<HTMLButtonElement>(
                              '#menuRemoveButton')!.disabled);
        });
  });

  test(history_list_test.TestNames.DeletingItemsUsingShortcuts, function() {
    let dialog: CrDialogElement;
    let items: NodeListOf<HistoryItemElement>;
    return finishSetup(TEST_HISTORY_RESULTS)
        .then(function() {
          dialog = element.$.dialog.get();
          flushTasks();
        })
        .then(function() {
          element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
              new CustomEvent('iron-resize', {bubbles: true, composed: true}));
          return waitAfterNextRender(element);
        })
        .then(function() {
          flush();
          items = element.shadowRoot!.querySelectorAll('history-item');

          // Dialog should not appear when there is no item selected.
          pressAndReleaseKeyOn(document.body, 46, '', 'Delete');
          return flushTasks();
        })
        .then(function() {
          assertFalse(dialog.open);

          items[1]!.$.checkbox.click();
          items[2]!.$.checkbox.click();

          assertEquals(2, toolbar.count);

          pressAndReleaseKeyOn(document.body, 46, '', 'Delete');
          return flushTasks();
        })
        .then(function() {
          assertTrue(dialog.open);
          element.shadowRoot!.querySelector<HTMLElement>(
                                 '.cancel-button')!.click();
          assertFalse(dialog.open);

          pressAndReleaseKeyOn(document.body, 8, '', 'Backspace');
          return flushTasks();
        })
        .then(function() {
          assertTrue(dialog.open);
          element.shadowRoot!.querySelector<HTMLElement>(
                                 '.action-button')!.click();
          return testService.whenCalled('removeVisits');
        })
        .then(function(toRemove) {
          assertEquals('https://www.example.com', toRemove[0].url);
          assertEquals('https://www.google.com', toRemove[1].url);
          assertEquals(
              Date.parse('2016-03-14 10:00 UTC'), toRemove[0].timestamps[0]);
          assertEquals(
              Date.parse('2016-03-14 9:00 UTC'), toRemove[1].timestamps[0]);
        });
  });

  test(
      history_list_test.TestNames.DeleteDialogClosedOnBackNavigation,
      function() {
        // Ensure that state changes are always mirrored to the URL.
        return finishSetup([])
            .then(function() {
              testService.resetResolver('queryHistory');
              app.shadowRoot!.querySelector('history-router')!.shadowRoot!
                  .querySelector('iron-location')!.dwellTime = 0;

              testService.setQueryResult({
                info: createHistoryInfo('something else'),
                value: TEST_HISTORY_RESULTS,
              });

              // Navigate from chrome://history/ to
              // chrome://history/?q=something else.
              app.dispatchEvent(new CustomEvent('change-query', {
                bubbles: true,
                composed: true,
                detail: {search: 'something else'},
              }));
              return testService.whenCalled('queryHistory');
            })
            .then(function() {
              testService.resetResolver('queryHistory');
              testService.setQueryResult({
                info: createHistoryInfo('something else'),
                value: ADDITIONAL_RESULTS,
              });
              element.dispatchEvent(new CustomEvent(
                  'query-history',
                  {bubbles: true, composed: true, detail: true}));
              return testService.whenCalled('queryHistoryContinuation');
            })
            .then(flushTasks)
            .then(function() {
              flush();
              const items =
                  element.shadowRoot!.querySelectorAll('history-item');

              items[2]!.$.checkbox.click();
              return flushTasks();
            })
            .then(function() {
              toolbar.deleteSelectedItems();
              return flushTasks();
            })
            .then(function() {
              // Confirmation dialog should appear.
              assertTrue(element.$.dialog.getIfExists()!.open);
              // Navigate back to chrome://history.
              window.history.back();

              return waitForEvent(window, 'popstate');
            })
            .then(flushTasks)
            .then(function() {
              assertFalse(element.$.dialog.getIfExists()!.open);
            });
      });

  test(
      history_list_test.TestNames.ClickingFileUrlSendsMessageToChrome,
      function() {
        const fileURL = 'file:///home/myfile';
        return finishSetup([createHistoryEntry('2016-03-15', fileURL)])
            .then(flushTasks)
            .then(function() {
              flush();
              const items =
                  element.shadowRoot!.querySelectorAll('history-item');
              items[0]!.$.link.click();
              return testService.whenCalled('navigateToUrl');
            })
            .then(function(url) {
              assertEquals(fileURL, url);
            });
      });

  test(
      history_list_test.TestNames.DeleteHistoryResultsInQueryHistoryEvent,
      function() {
        return finishSetup(TEST_HISTORY_RESULTS)
            .then(function() {
              testService.resetResolver('queryHistory');
              webUIListenerCallback('history-deleted');
            })
            .then(flushTasks)
            .then(function() {
              element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
                  new CustomEvent(
                      'iron-resize', {bubbles: true, composed: true}));
              return waitAfterNextRender(element);
            })
            .then(function() {
              flush();
              const items =
                  element.shadowRoot!.querySelectorAll('history-item');

              items[2]!.$.checkbox.click();
              items[3]!.$.checkbox.click();

              testService.resetResolver('queryHistory');
              webUIListenerCallback('history-deleted');
              flushTasks();
              assertEquals(0, testService.getCallCount('queryHistory'));
            });
      });

  teardown(function() {
    app.dispatchEvent(new CustomEvent(
        'change-query', {bubbles: true, composed: true, detail: {search: ''}}));
  });
});
