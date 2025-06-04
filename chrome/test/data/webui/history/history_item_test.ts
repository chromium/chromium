// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryEntry, HistoryItemElement, HistoryListElement} from 'chrome://history/history.js';
import {BrowserServiceImpl} from 'chrome://history/history.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry, createSearchEntry} from './test_util.js';

const TEST_HISTORY_RESULTS = [
  createHistoryEntry('2016-03-16 10:00', 'http://www.google.com'),
  createHistoryEntry('2016-03-16 9:00', 'http://www.example.com'),
  createHistoryEntry('2016-03-16 7:01', 'http://www.badssl.com'),
  createHistoryEntry('2016-03-16 7:00', 'http://www.website.com'),
  createHistoryEntry('2016-03-16 4:00', 'http://www.website.com'),
  createHistoryEntry('2016-03-15 11:00', 'http://www.example.com'),
];

const SEARCH_HISTORY_RESULTS = [
  createSearchEntry('2016-03-16', 'http://www.google.com'),
  createSearchEntry('2016-03-14 11:00', 'http://calendar.google.com'),
  createSearchEntry('2016-03-14 10:00', 'http://mail.google.com'),
];

suite('<history-item> unit test', function() {
  let item: HistoryItemElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserServiceImpl.setInstance(new TestBrowserService());

    item = document.createElement('history-item');
    item.item = TEST_HISTORY_RESULTS[0]!;
    document.body.appendChild(item);
  });

  test('click targets for selection', async function() {
    let selectionCount = 0;
    item.addEventListener('history-checkbox-select', function() {
      selectionCount++;
    });

    // Checkbox should trigger selection.
    item.$.checkbox.click();
    await microtasksFinished();
    assertEquals(1, selectionCount);

    // Non-interactive text should trigger selection.
    item.$['time-accessed'].click();
    assertEquals(2, selectionCount);

    // Menu button should not trigger selection.
    item.$['menu-button'].click();
    assertEquals(2, selectionCount);
  });

  test('title changes with item', async function() {
    const time = item.$['time-accessed'];
    assertEquals('', time.title);

    time.dispatchEvent(new CustomEvent('mouseover'));
    const initialTitle = time.title;
    item.item = TEST_HISTORY_RESULTS[5]!;
    await microtasksFinished();
    time.dispatchEvent(new CustomEvent('mouseover'));
    assertNotEquals(initialTitle, time.title);
  });
});

suite('<history-item> integration test', function() {
  let element: HistoryListElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);
    // Force a super tall body so that cr-lazy-list renders all items.
    document.body.style.height = '1000px';
    const app = document.createElement('history-app');
    document.body.appendChild(app);
    element = app.$.history;
    return testService.handler.whenCalled('queryHistory');
  });

  function getHistoryData(): HistoryEntry[] {
    return (element.$.infiniteList.items || []) as HistoryEntry[];
  }

  test('basic separator insertion', async function() {
    element.addNewResults(TEST_HISTORY_RESULTS, false, true);
    await eventToPromise('viewport-filled', element.$.infiniteList);

    // Check that the correct number of time gaps are inserted.
    const items = element.shadowRoot.querySelectorAll('history-item');
    assertEquals(TEST_HISTORY_RESULTS.length, items.length);
    assertTrue(items[0]!.hasTimeGap);
    assertTrue(items[1]!.hasTimeGap);
    assertFalse(items[2]!.hasTimeGap);
    assertTrue(items[3]!.hasTimeGap);
    assertFalse(items[4]!.hasTimeGap);
    assertFalse(items[5]!.hasTimeGap);
  });

  test('separator insertion for search', function() {
    element.addNewResults(SEARCH_HISTORY_RESULTS, false, true);
    element.searchedTerm = 'search';

    return microtasksFinished().then(function() {
      const items = element.shadowRoot.querySelectorAll('history-item');

      assertTrue(items[0]!.hasTimeGap);
      assertFalse(items[1]!.hasTimeGap);
      assertFalse(items[2]!.hasTimeGap);
    });
  });

  test('separator insertion after deletion', async function() {
    element.addNewResults(TEST_HISTORY_RESULTS, false, true);
    await microtasksFinished();
    const items = element.shadowRoot.querySelectorAll('history-item');

    element.removeItemsByIndexForTesting([3]);
    await microtasksFinished();

    // Checks that a new time gap separator has been inserted.
    assertEquals(5, getHistoryData().length);
    assertTrue(items[2]!.hasTimeGap);

    element.removeItemsByIndexForTesting([3]);
    await flushTasks();

    // Checks time gap separator is removed.
    assertEquals(4, element.$.infiniteList.items.length);
    assertFalse(items[2]!.hasTimeGap);
  });

  test('remove bookmarks', async function() {
    const newResults = [...TEST_HISTORY_RESULTS];
    newResults[1]!.starred = true;
    newResults[5]!.starred = true;
    element.addNewResults(newResults, false, true);
    await microtasksFinished();

    const items = element.shadowRoot.querySelectorAll('history-item');
    assertEquals(TEST_HISTORY_RESULTS.length, items.length);
    const star =
        items[1]!.shadowRoot.querySelector<HTMLElement>('#bookmark-star');
    assertTrue(!!star);
    star.focus();
    star.click();
    await microtasksFinished();

    // Check that all items matching this url are unstarred.
    assertEquals(getHistoryData()[1]!.starred, false);
    assertEquals(getHistoryData()[5]!.starred, false);
  });
});
