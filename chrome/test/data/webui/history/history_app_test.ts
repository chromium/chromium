// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, CrRouter} from 'chrome://history/history.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';

suite('HistoryAppTest', function() {
  let element: HistoryAppElement;
  let browserService: TestBrowserService;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserService = new TestBrowserService();
    BrowserServiceImpl.setInstance(browserService);
    // Some of the tests below assume the query state is fully reset to empty
    // between tests.
    window.history.replaceState({}, '', '/');
    CrRouter.resetForTesting();
    element = document.createElement('history-app');
    document.body.appendChild(element);
    return flushTasks();
  });

  test('SetsScrollTarget', async () => {
    assertEquals(element.$.tabsScrollContainer, element.scrollTarget);

    // 'By group' view shares the same scroll container as default history view.
    element.$.router.selectedPage = 'grouped';
    await flushTasks();
    assertEquals(element.$.tabsScrollContainer, element.scrollTarget);

    // Switching to synced tabs should change scroll target to it.
    element.$.router.selectedPage = 'syncedTabs';
    await flushTasks();
    assertEquals(
        element.shadowRoot!.querySelector('history-synced-device-manager'),
        element.scrollTarget);
  });

  test('ShowsHistoryEmbeddings', async () => {
    // By default, embeddings should not even be in the DOM.
    assertFalse(!!element.shadowRoot!.querySelector('cr-history-embeddings'));

    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'one'}}));
    await flushTasks();
    assertFalse(!!element.shadowRoot!.querySelector('cr-history-embeddings'));

    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'two words'}}));
    await flushTasks();
    assertTrue(!!element.shadowRoot!.querySelector('cr-history-embeddings'));

    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'one'}}));
    await flushTasks();
    assertFalse(!!element.shadowRoot!.querySelector('cr-history-embeddings'));
  });

  test('SetsScrollOffset', async () => {
    function resizeAndWait(height: number) {
      const historyEmbeddingsContainer =
          element.shadowRoot!.querySelector<HTMLElement>(
              '#historyEmbeddingsContainer');
      assertTrue(!!historyEmbeddingsContainer);

      return new Promise<void>((resolve) => {
        const observer = new ResizeObserver(() => {
          if (historyEmbeddingsContainer.offsetHeight === height) {
            resolve();
            observer.unobserve(historyEmbeddingsContainer);
          }
        });
        observer.observe(historyEmbeddingsContainer);
        historyEmbeddingsContainer.style.height = `${height}px`;
      });
    }

    await resizeAndWait(700);
    await flushTasks();
    assertEquals(700, element.$.history.scrollOffset);

    await resizeAndWait(400);
    await flushTasks();
    assertEquals(400, element.$.history.scrollOffset);
  });

  test('QueriesMoreFromSiteFromHistoryEmbeddings', async () => {
    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'two words'}}));
    await flushTasks();
    const historyEmbeddings =
        element.shadowRoot!.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddings);

    const changeQueryEventPromise = eventToPromise('change-query', element);
    historyEmbeddings.dispatchEvent(new CustomEvent('more-from-site-click', {
      detail: {
        title: 'Google',
        url: {url: 'http://google.com'},
        urlForDisplay: 'google.com',
        relativeTime: '2 hours ago',
        sourcePassage: 'Google description',
        lastUrlVisitTimestamp: 1000,
      },
    }));
    const changeQueryEvent = await changeQueryEventPromise;
    assertEquals('host:google.com', changeQueryEvent.detail.search);
  });

  test('RemovesItemFromHistoryEmbeddings', async () => {
    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'two words'}}));
    await flushTasks();
    const historyEmbeddings =
        element.shadowRoot!.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddings);

    historyEmbeddings.dispatchEvent(new CustomEvent('remove-item-click', {
      detail: {
        title: 'Google',
        url: {url: 'http://google.com'},
        urlForDisplay: 'google.com',
        relativeTime: '2 hours ago',
        sourcePassage: 'Google description',
        lastUrlVisitTimestamp: 1000,
      },
    }));
    const removeVisitsArg = await browserService.whenCalled('removeVisits');
    assertEquals(1, removeVisitsArg.length);
    assertEquals('http://google.com', removeVisitsArg[0].url);
    assertEquals(1, removeVisitsArg[0].timestamps.length);
    assertEquals(1000, removeVisitsArg[0].timestamps[0]);
  });

  test('ChangesQueryStateWithFilterChips', async () => {
    const filterChips = element.shadowRoot!.querySelector(
        'cr-history-embeddings-filter-chips')!;
    const changeQueryEventPromise = eventToPromise('change-query', element);
    filterChips.dispatchEvent(new CustomEvent('selected-suggestion-changed', {
      detail: {
        value: {
          timeRangeStart: new Date('2011-01-01T00:00:00'),
        },
      },
      composed: true,
      bubbles: true,
    }));
    const changeQueryEvent = await changeQueryEventPromise;
    assertEquals('', changeQueryEvent.detail.search);
    assertEquals('2011-01-01', changeQueryEvent.detail.after);
  });

  test('UpdatesBindingsOnChangeQuery', async () => {
    // Change query to a multi-word search term and an after date.
    element.dispatchEvent(new CustomEvent('change-query', {
      bubbles: true,
      composed: true,
      detail: {
        search: 'two words',
        after: '2022-04-02',
      },
    }));
    await flushTasks();

    const expectedDateObject = new Date('2022-04-02T00:00:00');

    const filterChips = element.shadowRoot!.querySelector(
        'cr-history-embeddings-filter-chips')!;
    assertTrue(!!filterChips);
    assertEquals(
        expectedDateObject.getTime(), filterChips.timeRangeStart?.getTime());

    const historyEmbeddings =
        element.shadowRoot!.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddings);
    const timeRangeStartObj = historyEmbeddings.timeRangeStart;
    assertTrue(!!timeRangeStartObj);
    assertEquals(expectedDateObject.getTime(), timeRangeStartObj.getTime());

    // Update only the search term. Verify that the date object has not changed.
    element.dispatchEvent(new CustomEvent('change-query', {
      bubbles: true,
      composed: true,
      detail: {
        search: 'two words updated',
        after: '2022-04-02',
      },
    }));
    await flushTasks();
    assertEquals(timeRangeStartObj, historyEmbeddings.timeRangeStart);

    // Clear the after date query.
    element.dispatchEvent(new CustomEvent('change-query', {
      bubbles: true,
      composed: true,
      detail: {
        search: 'two words',
      },
    }));
    await flushTasks();
    assertEquals(undefined, historyEmbeddings.timeRangeStart);
  });
});
