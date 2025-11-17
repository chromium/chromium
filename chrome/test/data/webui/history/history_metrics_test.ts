// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement, HistoryEntry, HistoryItemElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, HistoryPageViewHistogram, HistorySignInState, SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram, VisitContextMenuAction} from 'chrome://history/history.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry, createHistoryInfo, createSession, createWindow, disableLinkClicks, navigateTo} from './test_util.js';

suite('Metrics', function() {
  let testService: TestBrowserService;
  let app: HistoryAppElement;
  let histogramMap: {[key: string]: {[key: string]: number}};
  let actionMap: {[key: string]: number};

  suiteSetup(function() {
    loadTimeData.overrideValues(
        {enableBrowsingHistoryActorIntegrationM1: true});

    disableLinkClicks();
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Make viewport tall enough to render all items.
    document.body.style.height = '1000px';

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
  async function finishSetup(
      queryResults: HistoryEntry[], query?: string): Promise<void> {
    testService.handler.setResultFor('queryHistory', Promise.resolve({
      results: {info: createHistoryInfo(query), value: queryResults},
    }));
    document.body.appendChild(app);
    await testService.handler.whenCalled('queryHistory');
    webUIListenerCallback(
        'sign-in-state-changed', HistorySignInState.SIGNED_OUT);
    return microtasksFinished();
  }

  /**
   * @param historyItem The history item element to open the context menu on.
   * @param buttonId The id of the button inside the context menu to perform a
   *     click on.
   */
  async function contextMenuButtonClick(
      historyItem: HistoryItemElement, buttonId: string) {
    historyItem.$['menu-button'].click();
    await microtasksFinished();

    const sharedMenu = app.$.history.$.sharedMenu.get();
    const button = sharedMenu.querySelector<HTMLElement>(buttonId);
    assertTrue(!!button);
    button.click();
    await microtasksFinished();
  }

  test('History.HistoryPageView', async () => {
    await finishSetup([]);

    const histogram = histogramMap['History.HistoryPageView'];
    assertTrue(!!histogram);
    assertEquals(1, histogram[HistoryPageViewHistogram.HISTORY]);

    navigateTo('/syncedTabs', app);
    await microtasksFinished();
    assertEquals(1, histogram[HistoryPageViewHistogram.SIGNIN_PROMO]);
    await testService.whenCalled('otherDevicesInitialized');

    testService.resetResolver('recordHistogram');
    webUIListenerCallback(
        'sign-in-state-changed', HistorySignInState.SIGNED_IN_SYNCING_TABS);
    await testService.whenCalled('recordHistogram');

    assertEquals(1, histogram[HistoryPageViewHistogram.SYNCED_TABS]);
    navigateTo('/history', app);
    await microtasksFinished();
    assertEquals(2, histogram[HistoryPageViewHistogram.HISTORY]);
  });

  test.skip('history-list', async () => {
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

    let items = app.$.history.shadowRoot.querySelectorAll('history-item');
    assertTrue(!!items[1]);
    items[1].shadowRoot.querySelector<HTMLElement>('#bookmark-star')!.click();
    assertEquals(1, actionMap['BookmarkStarClicked']);
    items[1].$.link.click();
    assertEquals(1, actionMap['EntryLinkClick']);

    testService.handler.resetResolver('queryHistory');
    testService.handler.setResultFor('queryHistory', Promise.resolve({
      results: {
        info: createHistoryInfo('goog'),
        value: [
          createHistoryEntry(weekAgo.getTime(), 'http://www.google.com'),
          createHistoryEntry(weekAgo.getTime(), 'http://www.google.com'),
          createHistoryEntry(weekAgo.getTime(), 'http://www.google.com'),
        ],
      },
    }));

    app.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'goog'}}));
    assertEquals(1, actionMap['Search']);
    const queryManager = app.shadowRoot.querySelector('history-query-manager');
    assertTrue(!!queryManager);
    queryManager.queryState = {...queryManager.queryState, incremental: true};
    await microtasksFinished();
    await testService.handler.whenCalled('queryHistory'),
        await eventToPromise('viewport-filled', app.$.history);
    await microtasksFinished();

    items = app.$.history.shadowRoot.querySelectorAll('history-item');
    assertTrue(!!items[0]);
    assertTrue(!!items[4]);
    // items[0].$.link.click();
    // await microtasksFinished();
    // assertEquals(1, actionMap['SearchResultClick']);
    // items[0].$.checkbox.click();
    // items[4].$.checkbox.click();
    // await microtasksFinished();

    // app.$.toolbar.deleteSelectedItems();
    // await microtasksFinished();
    // assertEquals(1, actionMap['RemoveSelected']);

    // app.$.history.shadowRoot.querySelector<HTMLElement>(
    //                             '.cancel-button')!.click();
    // await microtasksFinished();
    // assertEquals(1, actionMap['CancelRemoveSelected']);
    // app.$.toolbar.deleteSelectedItems();
    // await microtasksFinished();

    // testService.handler.setResultFor('removeVisits', Promise.resolve());
    // app.$.history.shadowRoot.querySelector<HTMLElement>(
    //                             '.action-button')!.click();
    // await microtasksFinished();
    // assertEquals(1, actionMap['ConfirmRemoveSelected']);

    // items = app.$.history.shadowRoot.querySelectorAll('history-item');
    // assertTrue(!!items[0]);
    // items[0].$['menu-button'].click();
    // await microtasksFinished();

    // app.$.history.shadowRoot.querySelector<HTMLElement>(
    //                             '#menuRemoveButton')!.click();
    // await Promise.all([
    //   testService.handler.whenCalled('removeVisits'),
    //   microtasksFinished(),
    // ]);
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
    await microtasksFinished();

    navigateTo('/syncedTabs', app);
    await microtasksFinished();

    const histogram = histogramMap[SYNCED_TABS_HISTOGRAM_NAME];
    assertTrue(!!histogram);
    assertEquals(1, histogram[SyncedTabsHistogram.INITIALIZED]);

    await testService.whenCalled('getForeignSessions');
    await microtasksFinished();

    assertEquals(1, histogram[SyncedTabsHistogram.HAS_FOREIGN_DATA]);

    const syncedDeviceManager =
        app.shadowRoot.querySelector('history-synced-device-manager');
    assertTrue(!!syncedDeviceManager);

    const cards = syncedDeviceManager.shadowRoot.querySelectorAll(
        'history-synced-device-card');
    assertTrue(!!cards[0]);
    cards[0].$['card-heading'].click();
    assertEquals(1, histogram[SyncedTabsHistogram.COLLAPSE_SESSION]);
    cards[0].$['card-heading'].click();
    assertEquals(1, histogram[SyncedTabsHistogram.EXPAND_SESSION]);
    cards[0].shadowRoot.querySelectorAll<HTMLElement>(
                           '.website-link')[0]!.click();
    assertEquals(1, histogram[SyncedTabsHistogram.LINK_CLICKED]);

    const menuButton = cards[0].$['menu-button'];
    menuButton.click();
    await microtasksFinished();

    syncedDeviceManager.shadowRoot
        .querySelector<HTMLElement>('#menuOpenButton')!.click();
    assertEquals(1, histogram[SyncedTabsHistogram.OPEN_ALL]);

    menuButton.click();
    await microtasksFinished();

    syncedDeviceManager.shadowRoot
        .querySelector<HTMLElement>('#menuDeleteButton')!.click();
    assertEquals(1, histogram[SyncedTabsHistogram.HIDE_FOR_NOW]);
  });

  test('history-clusters-duration', async () => {
    await finishSetup([]);

    navigateTo('/grouped', app);
    await microtasksFinished();

    navigateTo('/history', app);
    await microtasksFinished();

    const args = await testService.whenCalled('recordLongTime');
    assertEquals(args[0], 'History.Clusters.WebUISessionDuration');
  });

  test('history-list-with-actor-visit', async () => {
    // History page loaded with no actor-annotated visit.
    await finishSetup([
      createHistoryEntry('2025-08-27 10:00', 'http://www.example.com'),
      createHistoryEntry('2025-08-26 10:00', 'http://www.google.com'),
    ]);
    await microtasksFinished();

    const recordedHistograms1 =
        await testService.getArgs('recordBooleanHistogram');
    assertEquals(1, recordedHistograms1.length);
    assertEquals('HistoryPage.ActorItemsShown', recordedHistograms1[0][0]);
    assertFalse(recordedHistograms1[0][1]);

    const historyEntry =
        createHistoryEntry('2025-08-26 10:00', 'http://www.google.com');
    historyEntry.isActorVisit = true;

    // History page re-loaded with actor-annotated visits.
    testService.handler.resetResolver('queryHistory');
    testService.handler.setResultFor(
        'queryHistoryContinuation', Promise.resolve({
          results: {
            info: createHistoryInfo(),
            value: [
              historyEntry,
              historyEntry,
              createHistoryEntry('2025-08-25 10:00', 'http://www.example.com'),
            ],
          },
        }));
    app.dispatchEvent(new CustomEvent(
        'query-history', {detail: true, bubbles: true, composed: true}));
    await testService.handler.whenCalled('queryHistoryContinuation');

    const recordedHistogram2 =
        await testService.getArgs('recordBooleanHistogram');
    assertEquals(2, recordedHistogram2.length);
    assertEquals('HistoryPage.ActorItemsShown', recordedHistogram2[1][0]);
    assertTrue(recordedHistogram2[1][1]);
  });

  test('more-button-clicked-for-actor-visit', async () => {
    const historyEntry =
        createHistoryEntry('2025-08-26 10:00', 'http://www.google.com');
    historyEntry.isActorVisit = true;
    await finishSetup([historyEntry]);
    await microtasksFinished();

    const item = app.$.history.shadowRoot.querySelector('history-item');
    assertTrue(!!item);
    await contextMenuButtonClick(item, '#menuMoreButton');

    const histogram = histogramMap['HistoryPage.ActorContextMenuActions'];
    assertTrue(!!histogram);
    assertEquals(
        1, histogram[VisitContextMenuAction.MORE_FROM_THIS_SITE_CLICKED]);
  });

  test('more-button-clicked-for-non-actor-visit', async () => {
    await finishSetup(
        [createHistoryEntry('2025-08-26 10:00', 'http://www.google.com')]);
    await microtasksFinished();

    const item = app.$.history.shadowRoot.querySelector('history-item');
    assertTrue(!!item);
    await contextMenuButtonClick(item, '#menuMoreButton');

    const histogram = histogramMap['HistoryPage.NonActorContextMenuActions'];
    assertTrue(!!histogram);
    assertEquals(
        1, histogram[VisitContextMenuAction.MORE_FROM_THIS_SITE_CLICKED]);
  });

  test('remove-history-button-clicked-for-actor-visit', async () => {
    // Resolve `removeVisits` call so that the #menuRemoveButton click is
    // handled correctly.
    testService.handler.setResultFor('removeVisits', Promise.resolve());
    const historyEntry =
        createHistoryEntry('2025-08-26 10:00', 'http://www.google.com');
    historyEntry.isActorVisit = true;
    await finishSetup([historyEntry]);
    await microtasksFinished();

    const item = app.$.history.shadowRoot.querySelector('history-item');
    assertTrue(!!item);
    await contextMenuButtonClick(item, '#menuRemoveButton');

    const histogram = histogramMap['HistoryPage.ActorContextMenuActions'];
    assertTrue(!!histogram);
    assertEquals(
        1, histogram[VisitContextMenuAction.REMOVE_FROM_HISTORY_CLICKED]);
  });

  test('remove-history-button-clicked-for-non-actor-visit', async () => {
    // Resolve `removeVisits` call so that the #menuRemoveButton click is
    // handled correctly.
    testService.handler.setResultFor('removeVisits', Promise.resolve());
    await finishSetup(
        [createHistoryEntry('2025-08-26 10:00', 'http://www.google.com')]);
    await microtasksFinished();

    const item = app.$.history.shadowRoot.querySelector('history-item');
    assertTrue(!!item);
    await contextMenuButtonClick(item, '#menuRemoveButton');

    const histogram = histogramMap['HistoryPage.NonActorContextMenuActions'];
    assertTrue(!!histogram);
    assertEquals(
        1, histogram[VisitContextMenuAction.REMOVE_FROM_HISTORY_CLICKED]);
  });

  test('remove-bookmark-button-clicked-for-actor-visit', async () => {
    const historyEntry =
        createHistoryEntry('2025-08-26 10:00', 'http://www.google.com');
    historyEntry.starred = true;
    historyEntry.isActorVisit = true;
    await finishSetup([historyEntry]);
    await microtasksFinished();

    const item = app.$.history.shadowRoot.querySelector('history-item');
    assertTrue(!!item);
    await contextMenuButtonClick(item, '#menuRemoveBookmarkButton');

    const histogram = histogramMap['HistoryPage.ActorContextMenuActions'];
    assertTrue(!!histogram);
    assertEquals(1, histogram[VisitContextMenuAction.REMOVE_BOOKMARK_CLICKED]);
  });

  test('remove-bookmark-button-clicked-for-non-actor-visit', async () => {
    const historyEntry =
        createHistoryEntry('2025-08-26 10:00', 'http://www.google.com');
    historyEntry.starred = true;
    await finishSetup([historyEntry]);
    await microtasksFinished();

    const item = app.$.history.shadowRoot.querySelector('history-item');
    assertTrue(!!item);
    await contextMenuButtonClick(item, '#menuRemoveBookmarkButton');

    const histogram = histogramMap['HistoryPage.NonActorContextMenuActions'];
    assertTrue(!!histogram);
    assertEquals(1, histogram[VisitContextMenuAction.REMOVE_BOOKMARK_CLICKED]);
  });
});
