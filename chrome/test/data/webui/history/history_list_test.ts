// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HistoryAppElement, HistoryEntry, HistoryItemElement, HistoryListElement, HistoryToolbarElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, CrRouter, ensureLazyLoaded} from 'chrome://history/history.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry, createHistoryInfo, shiftClick, waitForEvent} from './test_util.js';

suite('HistoryListTest', function() {
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
    CrRouter.resetForTesting();
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

  test('IsEmpty', async () => {
    await finishSetup([]);
    await flushTasks();
    assertTrue(element.isEmpty);

    // Load some results.
    testService.resetResolver('queryHistory');
    testService.setQueryResult(
        {info: createHistoryInfo(), value: ADDITIONAL_RESULTS});
    element.dispatchEvent(new CustomEvent(
        'query-history', {detail: true, bubbles: true, composed: true}));
    await testService.whenCalled('queryHistoryContinuation');
    await flushTasks();

    assertFalse(element.isEmpty);
  });

  test('DeletingSingleItem', async function() {
    await finishSetup([createHistoryEntry('2015-01-01', 'http://example.com')]);
    await flushTasks();
    assertEquals(getHistoryData().length, 1);
    flush();
    const items = element.shadowRoot!.querySelectorAll('history-item');

    assertEquals(1, items.length);
    items[0]!.$.checkbox.click();
    await items[0]!.$.checkbox.updateComplete;
    assertDeepEquals([true], getHistoryData().map(i => i.selected));
    await flushTasks();
    toolbar.deleteSelectedItems();
    await flushTasks();
    const dialog = element.$.dialog.get();
    assertTrue(dialog.open);
    testService.resetResolver('queryHistory');
    element.shadowRoot!.querySelector<HTMLElement>('.action-button')!.click();
    const visits = await testService.whenCalled('removeVisits');
    assertEquals(1, visits.length);
    assertEquals('http://example.com', visits[0].url);
    assertEquals(Date.parse('2015-01-01 UTC'), visits[0].timestamps[0]);

    // The list should fire a query-history event which results in a
    // queryHistory call, since deleting the only item results in an
    // empty history list.
    return testService.whenCalled('queryHistory');
  });

  test('CancellingSelectionOfMultipleItems', async function() {
    await finishSetup(TEST_HISTORY_RESULTS);
    await flushTasks();
    element.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
    await waitAfterNextRender(element);
    flush();
    const items = element.shadowRoot!.querySelectorAll('history-item');

    items[2]!.$.checkbox.click();
    items[3]!.$.checkbox.click();

    await Promise.all([
      items[2]!.$.checkbox.updateComplete,
      items[3]!.$.checkbox.updateComplete,
    ]);

    // Make sure that the array of data that determines whether or not
    // an item is selected is what we expect after selecting the two
    // items.
    assertDeepEquals(
        [false, false, true, true], getHistoryData().map(i => i.selected));

    toolbar.clearSelectedItems();

    // Make sure that clearing the selection updates both the array
    // and the actual history-items affected.
    assertDeepEquals(
        [false, false, false, false], getHistoryData().map(i => i.selected));

    assertFalse(items[2]!.selected);
    assertFalse(items[3]!.selected);
  });

  test('SelectionOfMultipleItemsUsingShiftClick', async function() {
    await finishSetup(TEST_HISTORY_RESULTS);
    await flushTasks();
    element.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
    await waitAfterNextRender(element);
    flush();
    const items = element.shadowRoot!.querySelectorAll('history-item');

    items[1]!.$.checkbox.click();
    await items[1]!.$.checkbox.updateComplete;
    assertDeepEquals(
        [false, true, false, false], getHistoryData().map(i => i.selected));
    assertDeepEquals([1], Array.from(element.selectedItems).sort());

    // Shift-select to the last item.
    await shiftClick(items[3]!.$.checkbox);
    assertDeepEquals(
        [false, true, true, true], getHistoryData().map(i => i.selected));
    assertDeepEquals([1, 2, 3], Array.from(element.selectedItems).sort());

    // Shift-select back to the first item.
    await shiftClick(items[0]!.$.checkbox);
    assertDeepEquals(
        [true, true, true, true], getHistoryData().map(i => i.selected));
    assertDeepEquals([0, 1, 2, 3], Array.from(element.selectedItems).sort());

    // Shift-deselect to the third item.
    await shiftClick(items[2]!.$.checkbox);
    assertDeepEquals(
        [false, false, false, true], getHistoryData().map(i => i.selected));
    assertDeepEquals([3], Array.from(element.selectedItems).sort());

    // Select the second item.
    items[1]!.$.checkbox.click();
    await items[1]!.$.checkbox.updateComplete;
    assertDeepEquals(
        [false, true, false, true], getHistoryData().map(i => i.selected));
    assertDeepEquals([1, 3], Array.from(element.selectedItems).sort());

    // Shift-deselect to the last item.
    await shiftClick(items[3]!.$.checkbox);
    assertDeepEquals(
        [false, false, false, false], getHistoryData().map(i => i.selected));
    assertDeepEquals([], Array.from(element.selectedItems).sort());

    // Shift-select back to the third item.
    await shiftClick(items[2]!.$.checkbox);
    assertDeepEquals(
        [false, false, true, true], getHistoryData().map(i => i.selected));
    assertDeepEquals([2, 3], Array.from(element.selectedItems).sort());

    // Remove selected items.
    element.removeItemsForTest(Array.from(element.selectedItems));
    assertDeepEquals(
        ['https://www.google.com', 'https://www.example.com'],
        getHistoryData().map(i => i.title));
  });

  // See http://crbug.com/845802.
  test('DisablingCtrlAOnSyncedTabsPage', async function() {
    await finishSetup(TEST_HISTORY_RESULTS);
    app.shadowRoot!.querySelector('history-router')!.selectedPage =
        'syncedTabs';
    await flushTasks();
    const field = toolbar.$.mainToolbar.getSearchField();
    field.blur();
    assertFalse(field.showingSearch);

    const modifier = isMac ? 'meta' : 'ctrl';
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');

    assertDeepEquals(
        [false, false, false, false], getHistoryData().map(i => i.selected));
  });

  test('SettingFirstAndLastItems', async function() {
    await finishSetup(TEST_HISTORY_RESULTS);
    await flushTasks();
    element.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
    await waitAfterNextRender(element);
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

  async function loadWithAdditionalResults() {
    await finishSetup(TEST_HISTORY_RESULTS);
    testService.resetResolver('queryHistory');
    testService.setQueryResult(
        {info: createHistoryInfo(), value: ADDITIONAL_RESULTS});
    element.dispatchEvent(new CustomEvent(
        'query-history', {detail: true, bubbles: true, composed: true}));
    await testService.whenCalled('queryHistoryContinuation');
    return flushTasks();
  }

  test('UpdatingHistoryResults', async function() {
    await loadWithAdditionalResults();
    element.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
    await waitAfterNextRender(element);
    flush();
    const items = element.shadowRoot!.querySelectorAll('history-item');
    assertTrue(items[3]!.isCardStart);
    assertTrue(items[5]!.isCardEnd);

    assertTrue(items[6]!.isCardStart);
    assertTrue(items[6]!.isCardEnd);

    assertTrue(items[7]!.isCardStart);
    assertTrue(items[7]!.isCardEnd);
  });

  test('DeletingMultipleItemsFromView', async function() {
    await loadWithAdditionalResults();
    element.removeItemsForTest([2, 5, 7]);
    await flushTasks();
    element.shadowRoot!.querySelector('iron-list')!.fire('iron-resize');
    await waitAfterNextRender(element);
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

  test('SearchResultsDisplayWithCorrectItemTitle', async function() {
    await finishSetup(
        [createHistoryEntry('2016-03-15', 'https://www.google.com')]);
    element.searchedTerm = 'Google';
    await flushTasks();
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
    assertGT(title.children[1]!.innerHTML!.indexOf('<b>google</b>'), -1);
  });

  test('CorrectDisplayMessageWhenNoHistoryAvailable', async function() {
    await finishSetup([]);
    await flushTasks();
    assertFalse(element.$['no-results'].hidden);
    assertNotEquals('', element.$['no-results'].textContent!.trim());
    assertTrue(element.$['infinite-list'].hidden);

    testService.setQueryResult(
        {info: createHistoryInfo(), value: TEST_HISTORY_RESULTS});
    element.dispatchEvent(new CustomEvent(
        'query-history', {bubbles: true, composed: true, detail: false}));
    await testService.whenCalled('queryHistory');
    await flushTasks();
    assertTrue(element.$['no-results'].hidden);
    assertFalse(element.$['infinite-list'].hidden);
  });

  test('MoreFromThisSiteSendsAndSetsCorrectData', async function() {
    await finishSetup(TEST_HISTORY_RESULTS);
    await flushTasks();
    element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
        new CustomEvent('iron-resize', {bubbles: true, composed: true}));
    await waitAfterNextRender(element);
    flush();
    testService.resetResolver('queryHistory');
    testService.setQueryResult({
      info: createHistoryInfo('www.google.com'),
      value: TEST_HISTORY_RESULTS,
    });
    const items = element.shadowRoot!.querySelectorAll('history-item');
    items[0]!.$['menu-button'].click();
    element.$.sharedMenu.get();
    element.shadowRoot!.querySelector<HTMLElement>('#menuMoreButton')!.click();
    const query = await testService.whenCalled('queryHistory');
    assertEquals('host:www.google.com', query);
    await flushTasks();
    assertEquals(
        'host:www.google.com',
        toolbar.$.mainToolbar.getSearchField().getValue());

    element.$.sharedMenu.get().close();
    items[0]!.$['menu-button'].click();
    assertTrue(
        element.shadowRoot!.querySelector<HTMLElement>(
                               '#menuMoreButton')!.hidden);

    element.$.sharedMenu.get().close();
    items[1]!.$['menu-button'].click();
    assertFalse(
        element.shadowRoot!.querySelector<HTMLElement>(
                               '#menuMoreButton')!.hidden);
  });

  test('ChangingSearchDeselectsItems', async function() {
    await finishSetup(
        [createHistoryEntry('2016-06-9', 'https://www.example.com')], 'ex');
    await flushTasks();
    const item = element.shadowRoot!.querySelector('history-item')!;
    item.$.checkbox.click();
    await item.$.checkbox.updateComplete;

    assertEquals(1, toolbar.count);
    app.shadowRoot!.querySelector(
                       'history-query-manager')!.queryState.incremental = false;

    testService.resetResolver('queryHistory');
    testService.setQueryResult({
      info: createHistoryInfo('ample'),
      value: [createHistoryEntry('2016-06-9', 'https://www.example.com')],
    });
    element.dispatchEvent(new CustomEvent(
        'query-history', {bubbles: true, composed: true, detail: false}));
    await testService.whenCalled('queryHistory');
    assertEquals(0, toolbar.count);
  });

  test('DeleteItemsEndToEnd', async function() {
    await loadWithAdditionalResults();
    element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
        new CustomEvent('iron-resize', {bubbles: true, composed: true}));
    await waitAfterNextRender(element);
    const dialog = element.$.dialog.get();
    await flushTasks();
    let items = element.shadowRoot!.querySelectorAll('history-item');

    items[2]!.$.checkbox.click();
    items[5]!.$.checkbox.click();
    items[7]!.$.checkbox.click();

    await Promise.all([
      items[2]!.$.checkbox.updateComplete,
      items[5]!.$.checkbox.updateComplete,
      items[7]!.$.checkbox.updateComplete,
    ]);

    await flushTasks();
    toolbar.deleteSelectedItems();
    await flushTasks();
    testService.resetResolver('removeVisits');
    // Confirmation dialog should appear.
    assertTrue(dialog.open);
    element.shadowRoot!.querySelector<HTMLElement>('.action-button')!.click();
    const visits = await testService.whenCalled('removeVisits');
    assertEquals(3, visits.length);
    assertEquals(TEST_HISTORY_RESULTS[2]!.url, visits[0]!.url);
    assertEquals(
        TEST_HISTORY_RESULTS[2]!.allTimestamps[0], visits[0]!.timestamps[0]);
    assertEquals(ADDITIONAL_RESULTS[1]!.url, visits[1]!.url);
    assertEquals(
        ADDITIONAL_RESULTS[1]!.allTimestamps[0], visits[1]!.timestamps[0]);
    assertEquals(ADDITIONAL_RESULTS[3]!.url, visits[2]!.url);
    assertEquals(
        ADDITIONAL_RESULTS[3]!.allTimestamps[0], visits[2]!.timestamps[0]);
    await flushTasks();
    const historyData = getHistoryData();
    assertEquals(5, historyData.length);
    assertEquals(historyData[0]!.dateRelativeDay, '2016-03-15');
    assertEquals(historyData[2]!.dateRelativeDay, '2016-03-13');
    assertEquals(historyData[4]!.dateRelativeDay, '2016-03-11');
    assertFalse(dialog.open);

    flush();
    // Ensure the UI is correctly updated.
    items = element.shadowRoot!.querySelectorAll('history-item');

    assertEquals('https://www.google.com', items[0]!.item.title);
    assertEquals('https://www.example.com', items[1]!.item.title);
    assertEquals('https://en.wikipedia.org', items[2]!.item.title);
    assertEquals('https://en.wikipedia.org', items[3]!.item.title);
    assertEquals('https://www.google.com', items[4]!.item.title);
  });

  test('DeleteViaMenuButton', async function() {
    await finishSetup(TEST_HISTORY_RESULTS);
    await flushTasks();
    element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
        new CustomEvent('iron-resize', {bubbles: true, composed: true}));
    await waitAfterNextRender(element);
    flush();
    const items = element.shadowRoot!.querySelectorAll('history-item');
    items[1]!.$.checkbox.click();
    items[3]!.$.checkbox.click();

    await Promise.all([
      items[1]!.$.checkbox.updateComplete,
      items[3]!.$.checkbox.updateComplete,
    ]);

    items[1]!.$['menu-button'].click();

    element.$.sharedMenu.get();
    element.shadowRoot!.querySelector<HTMLElement>(
                           '#menuRemoveButton')!.click();
    const visits = await testService.whenCalled('removeVisits');
    assertEquals(1, visits.length);
    assertEquals(TEST_HISTORY_RESULTS[1]!.url, visits[0]!.url);
    assertEquals(
        TEST_HISTORY_RESULTS[1]!.allTimestamps[0], visits[0]!.timestamps[0]);
    await flushTasks();
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

  test('DeleteDisabledWhilePending', async function() {
    let items: NodeListOf<HistoryItemElement>;
    await finishSetup(TEST_HISTORY_RESULTS);
    testService.delayDelete();
    await flushTasks();
    element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
        new CustomEvent('iron-resize', {bubbles: true, composed: true}));
    await waitAfterNextRender(element);
    flush();
    items = element.shadowRoot!.querySelectorAll('history-item');
    items[1]!.$.checkbox.click();
    items[2]!.$.checkbox.click();
    await Promise.all([
      items[1]!.$.checkbox.updateComplete,
      items[2]!.$.checkbox.updateComplete,
    ]);
    items[1]!.$['menu-button'].click();
    element.$.sharedMenu.get();
    element.shadowRoot!.querySelector<HTMLElement>(
                           '#menuRemoveButton')!.click();
    const visits = await testService.whenCalled('removeVisits');
    assertEquals(1, visits.length);
    assertEquals(TEST_HISTORY_RESULTS[1]!.url, visits[0]!.url);
    assertEquals(
        TEST_HISTORY_RESULTS[1]!.allTimestamps[0], visits[0]!.timestamps[0]);

    // Deletion is still happening. Verify that menu button and toolbar
    // are disabled.
    assertTrue(
        element.shadowRoot!
            .querySelector<HTMLButtonElement>('#menuRemoveButton')!.disabled);
    assertEquals(2, toolbar.count);
    assertTrue(
        toolbar.shadowRoot!.querySelector('cr-toolbar-selection-overlay')!
            .querySelector('cr-button')!.disabled);

    // Key event should be ignored.
    assertEquals(1, testService.getCallCount('removeVisits'));
    pressAndReleaseKeyOn(document.body, 46, [], 'Delete');

    await flushTasks();
    assertEquals(1, testService.getCallCount('removeVisits'));
    testService.finishRemoveVisits();
    await flushTasks();
    // Reselect some items.
    items = element.shadowRoot!.querySelectorAll('history-item');
    items[1]!.$.checkbox.click();
    items[2]!.$.checkbox.click();

    await Promise.all([
      items[1]!.$.checkbox.updateComplete,
      items[2]!.$.checkbox.updateComplete,
    ]);

    // Check that delete option is re-enabled.
    assertEquals(2, toolbar.count);
    assertFalse(
        toolbar.shadowRoot!.querySelector('cr-toolbar-selection-overlay')!
            .querySelector('cr-button')!.disabled);

    // Menu button should also be re-enabled.
    items[1]!.$['menu-button'].click();
    element.$.sharedMenu.get();
    assertFalse(
        element.shadowRoot!
            .querySelector<HTMLButtonElement>('#menuRemoveButton')!.disabled);
  });

  test('DeletingItemsUsingShortcuts', async function() {
    await finishSetup(TEST_HISTORY_RESULTS);
    const dialog = element.$.dialog.get();
    await flushTasks();
    element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
        new CustomEvent('iron-resize', {bubbles: true, composed: true}));
    await waitAfterNextRender(element);
    flush();
    const items = element.shadowRoot!.querySelectorAll('history-item');

    // Dialog should not appear when there is no item selected.
    pressAndReleaseKeyOn(document.body, 46, [], 'Delete');
    await flushTasks();
    assertFalse(dialog.open);

    items[1]!.$.checkbox.click();
    items[2]!.$.checkbox.click();

    await Promise.all([
      items[1]!.$.checkbox.updateComplete,
      items[2]!.$.checkbox.updateComplete,
    ]);

    assertEquals(2, toolbar.count);

    pressAndReleaseKeyOn(document.body, 46, [], 'Delete');
    await flushTasks();
    assertTrue(dialog.open);
    element.shadowRoot!.querySelector<HTMLElement>('.cancel-button')!.click();
    assertFalse(dialog.open);

    pressAndReleaseKeyOn(document.body, 8, [], 'Backspace');
    await flushTasks();
    assertTrue(dialog.open);
    element.shadowRoot!.querySelector<HTMLElement>('.action-button')!.click();
    const toRemove = await testService.whenCalled('removeVisits');
    assertEquals('https://www.example.com', toRemove[0].url);
    assertEquals('https://www.google.com', toRemove[1].url);
    assertEquals(Date.parse('2016-03-14 10:00 UTC'), toRemove[0].timestamps[0]);
    assertEquals(Date.parse('2016-03-14 9:00 UTC'), toRemove[1].timestamps[0]);
  });

  test('DeleteDialogClosedOnBackNavigation', async function() {
    // Ensure that state changes are always mirrored to the URL.
    await finishSetup([]);
    testService.resetResolver('queryHistory');
    CrRouter.getInstance().setDwellTime(0);

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
    await testService.whenCalled('queryHistory');
    testService.resetResolver('queryHistory');
    testService.setQueryResult({
      info: createHistoryInfo('something else'),
      value: ADDITIONAL_RESULTS,
    });
    element.dispatchEvent(new CustomEvent(
        'query-history', {bubbles: true, composed: true, detail: true}));
    await testService.whenCalled('queryHistoryContinuation');
    await flushTasks();
    const items = element.shadowRoot!.querySelectorAll('history-item');

    items[2]!.$.checkbox.click();
    await items[2]!.$.checkbox.updateComplete;
    await flushTasks();
    toolbar.deleteSelectedItems();
    await flushTasks();
    // Confirmation dialog should appear.
    assertTrue(element.$.dialog.getIfExists()!.open);
    // Navigate back to chrome://history.
    window.history.back();

    await waitForEvent(window, 'popstate');
    await flushTasks();
    assertFalse(element.$.dialog.getIfExists()!.open);
  });

  test('ClickingFileUrlSendsMessageToChrome', async function() {
    const fileURL = 'file:///home/myfile';
    await finishSetup([createHistoryEntry('2016-03-15', fileURL)]);
    await flushTasks();
    const items = element.shadowRoot!.querySelectorAll('history-item');
    items[0]!.$.link.click();
    const url = await testService.whenCalled('navigateToUrl');
    assertEquals(fileURL, url);
  });

  test('DeleteHistoryResultsInQueryHistoryEvent', async function() {
    await finishSetup(TEST_HISTORY_RESULTS);
    testService.resetResolver('queryHistory');
    webUIListenerCallback('history-deleted');
    await flushTasks();
    element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
        new CustomEvent('iron-resize', {bubbles: true, composed: true}));
    await waitAfterNextRender(element);
    flush();
    const items = element.shadowRoot!.querySelectorAll('history-item');
    items[2]!.$.checkbox.click();
    items[3]!.$.checkbox.click();

    await Promise.all([
      items[2]!.$.checkbox.updateComplete,
      items[3]!.$.checkbox.updateComplete,
    ]);

    testService.resetResolver('queryHistory');
    webUIListenerCallback('history-deleted');
    await flushTasks();
    assertEquals(0, testService.getCallCount('queryHistory'));
  });

  test('SetsScrollTarget', async () => {
    await finishSetup(TEST_HISTORY_RESULTS);
    await flushTasks();
    assertEquals(app.scrollTarget, element.$['infinite-list'].scrollTarget);
    assertEquals(app.scrollTarget, element.$['scroll-threshold'].scrollTarget);
  });

  test('SetsScrollOffset', async () => {
    await finishSetup(TEST_HISTORY_RESULTS);
    await flushTasks();
    element.scrollOffset = 123;
    assertEquals(123, element.$['infinite-list'].scrollOffset);
  });

  teardown(function() {
    app.dispatchEvent(new CustomEvent(
        'change-query', {bubbles: true, composed: true, detail: {search: ''}}));
  });
});
