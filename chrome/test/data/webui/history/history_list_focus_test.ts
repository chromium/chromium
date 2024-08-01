// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HistoryAppElement, HistoryEntry, HistoryListElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, ensureLazyLoaded} from 'chrome://history/history.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);
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
          element.shadowRoot!.querySelector('iron-list')!.dispatchEvent(
              new CustomEvent('iron-resize', {bubbles: true, composed: true}));
          return waitAfterNextRender(element);
        });
  });

  function getHistoryData(): HistoryEntry[] {
    return element.shadowRoot!.querySelector('iron-list')!.items!;
  }

  test('list focus and keyboard nav', async () => {
    let focused;
    await flushTasks();
    flush();
    const items = element.shadowRoot!.querySelectorAll('history-item');

    items[2]!.$.checkbox.focus();
    focused = items[2]!.$.checkbox.getFocusableElement();

    // Wait for next render to ensure that focus handlers have been
    // registered (see HistoryItemElement.attached).
    await waitAfterNextRender(element);

    pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    flush();
    focused = items[2]!.$.link;
    assertEquals(focused, getDeepActiveElement());
    assertTrue(items[2]!.getFocusRow().isActive());
    assertFalse(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    flush();
    focused = items[3]!.$.link;
    assertEquals(focused, getDeepActiveElement());
    assertFalse(items[2]!.getFocusRow().isActive());
    assertTrue(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    flush();
    focused = items[3]!.$['menu-button'];
    assertEquals(focused, getDeepActiveElement());
    assertFalse(items[2]!.getFocusRow().isActive());
    assertTrue(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    flush();
    focused = items[2]!.$['menu-button'];
    assertEquals(focused, getDeepActiveElement());
    assertTrue(items[2]!.getFocusRow().isActive());
    assertFalse(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 37, [], 'ArrowLeft');
    flush();
    focused = items[2]!.shadowRoot!.querySelector('#bookmark-star')!;
    assertEquals(focused, getDeepActiveElement());
    assertTrue(items[2]!.getFocusRow().isActive());
    assertFalse(items[3]!.getFocusRow().isActive());

    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    flush();
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
    let keydownEvent = await promise;
    assertTrue(keydownEvent.defaultPrevented);

    assertDeepEquals(
        [true, true, true, true], getHistoryData().map(i => i.selected));

    // If everything is already selected, the same shortcut will trigger
    // cancelling selection.
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    assertDeepEquals(
        [false, false, false, false], getHistoryData().map(i => i.selected));

    // If the search field is focused, the keyboard event should be handled by
    // the browser (which triggers selection of the text within the search
    // input).
    field.getSearchInput().focus();
    promise = eventToPromise('keydown', document);
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    keydownEvent = await promise;
    assertFalse(keydownEvent.defaultPrevented);
    assertDeepEquals(
        [false, false, false, false], getHistoryData().map(i => i.selected));
  });

  test('deleting last item will focus on new last item', async () => {
    await flushTasks();
    flush();
    const items = element.shadowRoot!.querySelectorAll('history-item');
    assertEquals(4, getHistoryData().length);
    assertEquals(4, items.length);
    items[3]!.$['menu-button'].click();
    await flushTasks();
    element.shadowRoot!.querySelector<HTMLElement>(
                           '#menuRemoveButton')!.click();
    assertNotEquals(items[2]!.$['menu-button'], getDeepActiveElement());
    await testService.whenCalled('removeVisits');
    await flushTasks();
    assertEquals(3, getHistoryData().length);
    assertEquals(items[2]!.$['menu-button'], getDeepActiveElement());
  });
});
