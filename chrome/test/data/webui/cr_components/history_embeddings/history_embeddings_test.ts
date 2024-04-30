// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';

import {HistoryEmbeddingsBrowserProxyImpl} from 'chrome://resources/cr_components/history_embeddings/browser_proxy.js';
import type {HistoryEmbeddingsElement} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import {PageHandlerRemote} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import type {SearchResultItem} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('cr-history-embeddings', () => {
  let element: HistoryEmbeddingsElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  const mockResults: SearchResultItem[] = [
    {
      title: 'Google',
      url: {url: 'http://google.com'},
      urlForDisplay: 'google.com',
      relativeTime: '2 hours ago',
      sourcePassage: 'Google description',
      lastUrlVisitTimestamp: 1000,
    },
    {
      title: 'Youtube',
      url: {url: 'http://youtube.com'},
      urlForDisplay: 'youtube.com',
      relativeTime: '4 hours ago',
      sourcePassage: 'Youtube description',
      lastUrlVisitTimestamp: 2000,
    },
  ];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = TestMock.fromClass(PageHandlerRemote);
    HistoryEmbeddingsBrowserProxyImpl.setInstance(
        new HistoryEmbeddingsBrowserProxyImpl(handler));
    handler.setResultFor(
        'search', Promise.resolve({result: {items: [...mockResults]}}));

    element = document.createElement('cr-history-embeddings');
    document.body.appendChild(element);

    element.searchQuery = 'some query';
    await handler.whenCalled('search');
    return flushTasks();
  });

  test('Searches', async () => {
    assertEquals('some query', handler.getArgs('search')[0].query);
  });

  test('DisplaysHeading', async () => {
    loadTimeData.overrideValues({
      historyEmbeddingsHeading: 'searched for "$1"',
      historyEmbeddingsHeadingLoading: 'loading results for "$1"',
    });
    element.searchQuery = 'my query';
    assertEquals(
        'loading results for "my query"',
        element.$.heading.textContent!.trim());
    await handler.whenCalled('search');
    await flushTasks();
    assertEquals(
        'searched for "my query"', element.$.heading.textContent!.trim());
  });

  test('DisplaysResults', async () => {
    const resultsElements =
        element.shadowRoot!.querySelectorAll('cr-url-list-item');
    assertEquals(2, resultsElements.length);
    assertEquals('Google', resultsElements[0]!.title);
    assertEquals('Youtube', resultsElements[1]!.title);
  });

  test('FiresClick', async () => {
    const resultsElements =
        element.shadowRoot!.querySelectorAll('cr-url-list-item');
    const resultClickEventPromise = eventToPromise('result-click', element);
    resultsElements[0]!.click();
    const resultClickEvent = await resultClickEventPromise;
    assertEquals(mockResults[0], resultClickEvent.detail);
  });

  test('FiresClickOnMoreActions', async () => {
    const moreActionsIconButtons =
        element.shadowRoot!.querySelectorAll<HTMLElement>(
            'cr-url-list-item cr-icon-button');
    moreActionsIconButtons[0]!.click();
    await flushTasks();

    // Clicking on the more actions button for the first item should load
    // the cr-action-menu and open it.
    const moreActionsMenu = element.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!moreActionsMenu);
    assertTrue(moreActionsMenu.open);

    const actionMenuItems = moreActionsMenu.querySelectorAll('button');
    assertEquals(2, actionMenuItems.length);

    // Clicking on the first button should fire the 'more-from-site-click' event
    // with the first item's model, and then close the menu.
    const moreFromSiteEventPromise =
        eventToPromise('more-from-site-click', element);
    const moreFromSiteItem =
        moreActionsMenu.querySelector<HTMLElement>('#moreFromSiteOption')!;
    moreFromSiteItem.click();
    const moreFromSiteEvent = await moreFromSiteEventPromise;
    assertEquals(mockResults[0], moreFromSiteEvent.detail);
    assertFalse(moreActionsMenu.open);

    // Clicking on the second button should fire the 'remove-item-click' event
    // with the second item's model, and then close the menu.
    moreActionsIconButtons[1]!.click();
    assertTrue(moreActionsMenu.open);
    const removeItemEventPromise = eventToPromise('remove-item-click', element);
    const removeItemItem =
        moreActionsMenu.querySelector<HTMLElement>('#removeFromHistoryOption')!;
    removeItemItem.click();
    const removeItemEvent = await removeItemEventPromise;
    assertEquals(mockResults[1], removeItemEvent.detail);
    assertFalse(moreActionsMenu.open);
  });

  test('RemovesItemsFromFrontend', async () => {
    const moreActionsIconButtons =
        element.shadowRoot!.querySelectorAll<HTMLElement>(
            'cr-url-list-item cr-icon-button');

    // Open the 'more actions' menu for the first result and remove it.
    moreActionsIconButtons[0]!.click();
    element.shadowRoot!.querySelector<HTMLElement>(
                           '#removeFromHistoryOption')!.click();
    await flushTasks();

    // There is still 1 result left so it should still be visible.
    assertFalse(element.isEmpty);
    assertTrue(isVisible(element));
    assertEquals(
        1, element.shadowRoot!.querySelectorAll('cr-url-list-item').length);

    // Open the 'more actions' menu for the last result and remove it.
    moreActionsIconButtons[0]!.click();
    element.shadowRoot!.querySelector<HTMLElement>(
                           '#removeFromHistoryOption')!.click();
    await flushTasks();

    // No results left.
    assertTrue(element.isEmpty);
    assertFalse(isVisible(element));
  });
});
