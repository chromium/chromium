// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HistoryAppElement, HistoryEntry, HistoryListElement} from 'chrome://history/history.js';
import {BrowserServiceImpl} from 'chrome://history/history.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry, createHistoryInfo} from './test_util.js';

suite('<history-list>', function() {
  let app: HistoryAppElement;
  let element: HistoryListElement;
  let testService: TestBrowserService;
  const TEST_HISTORY_RESULTS = [
    createHistoryEntry('2016-03-15', 'https://www.google.com'),
    createHistoryEntry('2016-03-14 10:00', 'https://www.example.com'),
    createHistoryEntry('2016-03-14 9:00', 'https://www.google.com'),
    createHistoryEntry('2016-03-13', 'https://en.wikipedia.org'),
  ];
  TEST_HISTORY_RESULTS[2]!.starred = true;

  setup(function() {
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Make viewport tall enough to render all items.
    document.body.style.height = '1000px';
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);
    testService.handler.setResultFor('queryHistory', Promise.resolve({
      results: {
        info: createHistoryInfo(),
        value: TEST_HISTORY_RESULTS,
      },
    }));

    app = document.createElement('history-app');
    document.body.appendChild(app);
    element = app.$.history;
    return Promise.all([
      testService.handler.whenCalled('queryHistory'),
      microtasksFinished(),
      eventToPromise('viewport-filled', element.$.infiniteList),
    ]);
  });

  function getHistoryData(): HistoryEntry[] {
    return (element.$.infiniteList.items || []) as HistoryEntry[];
  }

  test('list focus and keyboard nav', async () => {
    let focused;
    const items = element.shadowRoot.querySelectorAll('history-item');
    assertEquals(TEST_HISTORY_RESULTS.length, items.length);
    items[2]!.$.checkbox.focus();
    focused = items[2]!.$.checkbox.getFocusableElement();

    pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    await microtasksFinished();
    focused = items[2]!.$.link;
    assertEquals(focused, getDeepActiveElement());
    assertTrue(items[2]!.getFocusRow().isActive());
    assertFalse(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    await microtasksFinished();
    focused = items[3]!.$.link;
    assertEquals(focused, getDeepActiveElement());
    assertFalse(items[2]!.getFocusRow().isActive());
    assertTrue(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    await microtasksFinished();
    focused = items[3]!.$['menu-button'];
    assertEquals(focused, getDeepActiveElement());
    assertFalse(items[2]!.getFocusRow().isActive());
    assertTrue(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    await microtasksFinished();
    focused = items[2]!.$['menu-button'];
    assertEquals(focused, getDeepActiveElement());
    assertTrue(items[2]!.getFocusRow().isActive());
    assertFalse(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 37, [], 'ArrowLeft');
    await microtasksFinished();
    focused = items[2]!.$.link;
    assertEquals(focused, getDeepActiveElement());
    assertTrue(items[2]!.getFocusRow().isActive());
    assertFalse(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    await microtasksFinished();
    focused = items[3]!.$.link;
    assertEquals(focused, getDeepActiveElement());
    assertFalse(items[2]!.getFocusRow().isActive());
    assertTrue(items[3]!.getFocusRow().isActive());
  });

  test('selection of all items using ctrl + a', async () => {
    const toolbar = app.$.toolbar;
    const field = toolbar.$.mainToolbar.getSearchField();
    field.blur();
    assertFalse(field.showingSearch);

    const modifier = isMac ? 'meta' : 'ctrl';
    let promise = eventToPromise('keydown', document);
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    await microtasksFinished();
    let keydownEvent = await promise;
    assertTrue(keydownEvent.defaultPrevented);

    assertDeepEquals(
        [true, true, true, true], getHistoryData().map(i => i.selected));

    // If everything is already selected, the same shortcut will trigger
    // cancelling selection.
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    await microtasksFinished();
    assertDeepEquals(
        [false, false, false, false], getHistoryData().map(i => i.selected));

    // If the search field is focused, the keyboard event should be handled by
    // the browser (which triggers selection of the text within the search
    // input).
    field.narrow = false;  // Force search input to be visible.
    await microtasksFinished();
    field.getSearchInput().focus();

    promise = eventToPromise('keydown', document);
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    keydownEvent = await promise;
    assertFalse(keydownEvent.defaultPrevented);
    assertDeepEquals(
        [false, false, false, false], getHistoryData().map(i => i.selected));
  });

  test('deleting last item will focus on new last item', async () => {
    testService.handler.setResultFor(
        'removeVisits', Promise.resolve([getHistoryData()[3]]));
    const items = element.shadowRoot.querySelectorAll('history-item');
    assertEquals(4, getHistoryData().length);
    assertEquals(4, items.length);
    items[3]!.$['menu-button'].click();
    await microtasksFinished();
    element.shadowRoot.querySelector<HTMLElement>('#menuRemoveButton')!.click();
    await microtasksFinished();
    assertNotEquals(items[2]!.$['menu-button'], getDeepActiveElement());
    await testService.handler.whenCalled('removeVisits');
    await microtasksFinished();
    assertEquals(3, getHistoryData().length);
    assertEquals(items[2]!.$['menu-button'], getDeepActiveElement());
  });
});
