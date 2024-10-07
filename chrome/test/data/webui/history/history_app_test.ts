// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, CrRouter, HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote} from 'chrome://history/history.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';

suite('HistoryAppTest', function() {
  let element: HistoryAppElement;
  let browserService: TestBrowserService;
  let embeddingsHandler: TestMock<HistoryEmbeddingsPageHandlerRemote>&
      HistoryEmbeddingsPageHandlerRemote;

  // Force cr-history-embeddings to be in the DOM for testing.
  async function forceHistoryEmbeddingsElement() {
    loadTimeData.overrideValues({historyEmbeddingsSearchMinimumWordCount: 0});
    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'some fake input'}}));
    return flushTasks();
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      historyEmbeddingsSearchMinimumWordCount: 2,
      enableHistoryEmbeddings: true,
      maybeShowEmbeddingsIph: false,
    });

    browserService = new TestBrowserService();
    BrowserServiceImpl.setInstance(browserService);
    embeddingsHandler = TestMock.fromClass(HistoryEmbeddingsPageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(embeddingsHandler));
    embeddingsHandler.setResultFor(
        'search', Promise.resolve({result: {items: []}}));

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

  test('SetsScrollTargetForEmbeddingsDisabled', async () => {
    // Override loadTimeData and re-create the element to disable history
    // embeddings.
    loadTimeData.overrideValues({enableHistoryEmbeddings: false});
    element.remove();
    element = document.createElement('history-app');
    document.body.appendChild(element);
    await flushTasks();

    // By default, the history-list should be its own scroll container.
    assertEquals(
        element.shadowRoot!.querySelector('history-list'),
        element.scrollTarget);

    // 'By group' view switches the scroll target to it.
    element.$.router.selectedPage = 'grouped';
    await flushTasks();
    assertEquals(
        element.shadowRoot!.querySelector('history-clusters'),
        element.scrollTarget);

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

  test('UsesMinWordCount', async () => {
    loadTimeData.overrideValues({historyEmbeddingsSearchMinimumWordCount: 4});
    element.dispatchEvent(new CustomEvent('change-query', {
      bubbles: true,
      composed: true,
      detail: {search: 'two words'},
    }));
    await flushTasks();

    let historyEmbeddings =
        element.shadowRoot!.querySelector('cr-history-embeddings');
    assertFalse(!!historyEmbeddings);

    element.dispatchEvent(new CustomEvent('change-query', {
      bubbles: true,
      composed: true,
      detail: {search: 'at least four words'},
    }));
    await flushTasks();
    historyEmbeddings =
        element.shadowRoot!.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddings);
  });

  test('CountsCharacters', async () => {
    await forceHistoryEmbeddingsElement();

    function dispatchNativeInput(
        inputEvent: Partial<InputEvent>, inputValue: string) {
      element.$.toolbar.dispatchEvent(new CustomEvent(
          'search-term-native-before-input', {detail: {e: inputEvent}}));
      element.$.toolbar.dispatchEvent(
          new CustomEvent('search-term-native-input', {
            detail: {e: inputEvent, inputValue},
            composed: true,
            bubbles: true,
          }));
    }

    function getCount() {
      const historyEmbeddingsElement =
          element.shadowRoot!.querySelector('cr-history-embeddings')!;
      return historyEmbeddingsElement.numCharsForQuery;
    }

    dispatchNativeInput({data: 'a'}, 'a');
    assertEquals(1, getCount(), 'counts normal characters');
    dispatchNativeInput({data: 'b'}, 'ab');
    dispatchNativeInput({data: 'c'}, 'abc');
    assertEquals(3, getCount(), 'counts additional characters');

    dispatchNativeInput({data: 'pasted text'}, 'pasted text');
    assertEquals(1, getCount(), 'insert that replaces all text counts as 1');

    dispatchNativeInput({data: 'more text'}, 'pasted text more text');
    assertEquals(
        2, getCount(), 'insert that adds to existing input increments count');

    dispatchNativeInput({data: null}, 'pasted text more tex');
    assertEquals(3, getCount(), 'deletion increments');

    dispatchNativeInput({data: null}, '');
    assertEquals(0, getCount(), 'deletion of entire input resets counter');

    element.$.toolbar.dispatchEvent(new CustomEvent('search-term-cleared'));
    assertEquals(0, getCount(), 'resets on clear');
  });

  test('RegistersAndMaybeShowsPromo', async () => {
    assertEquals(
        0, embeddingsHandler.getCallCount('maybeShowFeaturePromo'),
        'promo is disabled in setup');

    // Recreate the app with the promo enabled.
    loadTimeData.overrideValues({maybeShowEmbeddingsIph: true});
    element = document.createElement('history-app');
    document.body.appendChild(element);
    await flushTasks();
    assertDeepEquals(
        element.getSortedAnchorStatusesForTesting(),
        [
          ['kHistorySearchInputElementId', true],
        ],
    );
    await embeddingsHandler.whenCalled('maybeShowFeaturePromo');
    assertEquals(
        1, embeddingsHandler.getCallCount('maybeShowFeaturePromo'),
        'promo is disabled in setup');
  });

  test('ProductSpecsIncrementsToolbar', async () => {
    // Reset the app with product spec lists feature enabled.
    document.body.removeChild(element);
    loadTimeData.overrideValues({compareHistoryEnabled: true});
    element = document.createElement('history-app');
    document.body.appendChild(element);
    element.$.router.selectedPage = 'comparisonTables';
    await flushTasks();
    assertEquals(0, element.$.toolbar.count);

    const productSpecificationsList =
        element.shadowRoot!.querySelector('product-specifications-lists');
    assert(!!productSpecificationsList);

    // Mock adding a selected item.
    productSpecificationsList.selectedItems.add('uuid1');
    productSpecificationsList.dispatchEvent(
        new CustomEvent('product-spec-item-select', {
          bubbles: true,
          composed: true,
          detail: {
            checked: true,
            uuid: 'uuid1',
          },
        }));
    await flushTasks();

    assertEquals(1, element.$.toolbar.count);
  });

  test('ProductSpecsSelectUnselectAll', async () => {
    // Reset the app with product spec lists feature enabled.
    document.body.removeChild(element);
    loadTimeData.overrideValues({compareHistoryEnabled: true});
    element = document.createElement('history-app');
    document.body.appendChild(element);
    element.$.router.selectedPage = 'comparisonTables';
    await flushTasks();
    assertEquals(0, element.$.toolbar.count);

    // Stub the selectOrUnselectAll method in the list element.
    let selectAllCalled = false;
    const productSpecificationsList =
        element.shadowRoot!.querySelector('product-specifications-lists');
    assert(!!productSpecificationsList);
    productSpecificationsList.selectOrUnselectAll = function() {
      selectAllCalled = true;
    };

    // Mock ctrl+A.
    productSpecificationsList.selectedItems.add('uuid1');
    productSpecificationsList.selectedItems.add('uuid2');
    const modifier = isMac ? 'meta' : 'ctrl';
    pressAndReleaseKeyOn(document.body, 65, modifier, 'a');
    await flushTasks();

    assertEquals(true, selectAllCalled);
    assertEquals(2, element.$.toolbar.count);
  });

  test('PassesDisclaimerLinkClicksToEmbeddings', async () => {
    await forceHistoryEmbeddingsElement();
    const historyEmbeddingsElement =
        element.shadowRoot!.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddingsElement);
    assertFalse(historyEmbeddingsElement.forceSuppressLogging);
    element.$.historyEmbeddingsDisclaimerLink.click();
    assertTrue(historyEmbeddingsElement.forceSuppressLogging);
  });

  test('PassesDisclaimerLinkAuxClicksToEmbeddings', async () => {
    await forceHistoryEmbeddingsElement();
    const historyEmbeddingsElement =
        element.shadowRoot!.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddingsElement);
    assertFalse(historyEmbeddingsElement.forceSuppressLogging);
    element.$.historyEmbeddingsDisclaimerLink.dispatchEvent(
        new MouseEvent('auxclick'));
    assertTrue(historyEmbeddingsElement.forceSuppressLogging);
  });

  test('SetsDateTimeFormatForEmbeddings', async () => {
    await forceHistoryEmbeddingsElement();
    const historyEmbeddingsElement =
        element.shadowRoot!.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddingsElement);
    assertFalse(historyEmbeddingsElement.showRelativeTimes);

    element.$.router.selectedPage = 'grouped';
    await flushTasks();
    assertTrue(historyEmbeddingsElement.showRelativeTimes);

    element.$.router.selectedPage = 'history';
    await flushTasks();
    assertFalse(historyEmbeddingsElement.showRelativeTimes);
  });
});
