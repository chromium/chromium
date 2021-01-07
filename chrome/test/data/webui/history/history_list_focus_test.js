// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService, ensureLazyLoaded} from 'chrome://history/history.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {createHistoryEntry, createHistoryInfo, polymerSelectAll} from 'chrome://test/history/test_util.js';
import {eventToPromise, flushTasks, waitAfterNextRender} from 'chrome://test/test_util.m.js';

suite('<history-list>', function() {
  let app;
  let element;
  let testService;
  const TEST_HISTORY_RESULTS = [
    createHistoryEntry('2016-03-15', 'https://www.google.com'),
    createHistoryEntry('2016-03-14 10:00', 'https://www.example.com'),
    createHistoryEntry('2016-03-14 9:00', 'https://www.google.com'),
    createHistoryEntry('2016-03-13', 'https://en.wikipedia.org')
  ];
  TEST_HISTORY_RESULTS[2].starred = true;

  setup(function() {
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = '';
    testService = new TestBrowserService();
    BrowserService.instance_ = testService;
    testService.setQueryResult({
      info: createHistoryInfo(),
      value: TEST_HISTORY_RESULTS,
    });

    app = document.createElement('history-app');
    document.body.appendChild(app);
    element = app.$.history;
    return Promise
        .all([
          testService.whenCalled('queryHistory'),
          ensureLazyLoaded(),
        ])
        .then(flushTasks)
        .then(function() {
          element.$$('iron-list').fire('iron-resize');
          return waitAfterNextRender(element);
        });
  });

  test('list focus and keyboard nav', async () => {
    let focused;
    await flushTasks();
    flush();
    const items = polymerSelectAll(element, 'history-item');

    items[2].$.checkbox.focus();
    focused = items[2].$.checkbox.getFocusableElement();

    // Wait for next render to ensure that focus handlers have been
    // registered (see HistoryItemElement.attached).
    await waitAfterNextRender(this);

    pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    flush();
    focused = items[2].$.link;
    assertEquals(focused, element.lastFocused_);
    assertTrue(items[2].row_.isActive());
    assertFalse(items[3].row_.isActive());

    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    flush();
    focused = items[3].$.link;
    assertEquals(focused, element.lastFocused_);
    assertFalse(items[2].row_.isActive());
    assertTrue(items[3].row_.isActive());

    pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    flush();
    focused = items[3].$['menu-button'];
    assertEquals(focused, element.lastFocused_);
    assertFalse(items[2].row_.isActive());
    assertTrue(items[3].row_.isActive());

    pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    flush();
    focused = items[2].$['menu-button'];
    assertEquals(focused, element.lastFocused_);
    assertTrue(items[2].row_.isActive());
    assertFalse(items[3].row_.isActive());

    pressAndReleaseKeyOn(focused, 37, [], 'ArrowLeft');
    flush();
    focused = items[2].$$('#bookmark-star');
    assertEquals(focused, element.lastFocused_);
    assertTrue(items[2].row_.isActive());
    assertFalse(items[3].row_.isActive());

    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    flush();
    focused = items[3].$.link;
    assertEquals(focused, element.lastFocused_);
    assertFalse(items[2].row_.isActive());
    assertTrue(items[3].row_.isActive());
  });

  test('selection of all items using ctrl + a', async () => {
    const toolbar = app.$.toolbar;
    const field = toolbar.$['main-toolbar'].getSearchField();
    field.blur();
    assertFalse(field.showingSearch);

    const modifier = isMac ? 'meta' : 'ctrl';
    let promise = eventToPromise('keydown', document);
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    let keydownEvent = await promise;
    assertTrue(keydownEvent.defaultPrevented);

    assertDeepEquals(
        [true, true, true, true], element.historyData_.map(i => i.selected));

    // If everything is already selected, the same shortcut will trigger
    // cancelling selection.
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    assertDeepEquals(
        [false, false, false, false],
        element.historyData_.map(i => i.selected));

    // If the search field is focused, the keyboard event should be handled by
    // the browser (which triggers selection of the text within the search
    // input).
    field.getSearchInput().focus();
    promise = eventToPromise('keydown', document);
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    keydownEvent = await promise;
    assertFalse(keydownEvent.defaultPrevented);
    assertDeepEquals(
        [false, false, false, false],
        element.historyData_.map(i => i.selected));
  });

  test('deleting last item will focus on new last item', async () => {
    let focused;
    await flushTasks();
    flush();
    const items = polymerSelectAll(element, 'history-item');
    assertEquals(4, element.historyData_.length);
    assertEquals(4, items.length);
    items[3].$['menu-button'].click();
    await flushTasks();
    element.$$('#menuRemoveButton').click();
    assertNotEquals(items[2].$['menu-button'], element.lastFocused_);
    await testService.whenCalled('removeVisits');
    await flushTasks();
    assertEquals(3, element.historyData_.length);
    assertEquals(items[2].$['menu-button'], element.lastFocused_);
  });
});
