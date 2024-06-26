// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';

import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {HistoryEmbeddingsBrowserProxyImpl} from 'chrome://resources/cr_components/history_embeddings/browser_proxy.js';
import type {HistoryEmbeddingsElement} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import {PageHandlerRemote, UserFeedback} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import type {SearchResultItem} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

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
    element.overrideQueryResultMinAgeForTesting(0);
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

  test('SetsUserFeedback', async () => {
    assertEquals(
        CrFeedbackOption.UNSPECIFIED, element.$.feedbackButtons.selectedOption,
        'defaults to unspecified');

    function dispatchFeedbackOptionChange(option: CrFeedbackOption) {
      element.$.feedbackButtons.dispatchEvent(
          new CustomEvent('selected-option-changed', {
            bubbles: true,
            composed: true,
            detail: {value: option},
          }));
    }

    dispatchFeedbackOptionChange(CrFeedbackOption.THUMBS_DOWN);
    assertEquals(
        UserFeedback.kUserFeedbackNegative,
        await handler.whenCalled('setUserFeedback'));
    assertEquals(
        CrFeedbackOption.THUMBS_DOWN, element.$.feedbackButtons.selectedOption);
    handler.reset();

    dispatchFeedbackOptionChange(CrFeedbackOption.THUMBS_UP);
    assertEquals(
        UserFeedback.kUserFeedbackPositive,
        await handler.whenCalled('setUserFeedback'));
    assertEquals(
        CrFeedbackOption.THUMBS_UP, element.$.feedbackButtons.selectedOption);
    handler.reset();

    dispatchFeedbackOptionChange(CrFeedbackOption.UNSPECIFIED);
    assertEquals(
        UserFeedback.kUserFeedbackUnspecified,
        await handler.whenCalled('setUserFeedback'));
    assertEquals(
        CrFeedbackOption.UNSPECIFIED, element.$.feedbackButtons.selectedOption);
    handler.reset();

    // Set up a new query and result.
    handler.setResultFor(
        'search', Promise.resolve({result: {items: [...mockResults]}}));
    element.searchQuery = 'new query';

    await handler.whenCalled('search');
    await flushTasks();
    assertEquals(
        CrFeedbackOption.UNSPECIFIED, element.$.feedbackButtons.selectedOption,
        'defaults back to unspecified when there is a new set of results');
  });

  test('SendsQualityLog', async () => {
    // Click on the second result.
    const resultsElements =
        element.shadowRoot!.querySelectorAll('cr-url-list-item');
    resultsElements[1]!.click();

    // Perform a new search, which should log the previous result.
    element.searchQuery = 'some new query';
    await handler.whenCalled('search');
    const clickedIndices = await handler.whenCalled('sendQualityLog');
    assertDeepEquals([1], clickedIndices);
    handler.resetResolver('sendQualityLog');

    // Override the minimum result age and ensure transient results are not
    // logged. Only after the 100ms passes and another search is performed
    // should the quality log be sent.
    element.overrideQueryResultMinAgeForTesting(100);
    element.searchQuery = 'some newer que';
    await handler.whenCalled('search');
    element.searchQuery = 'some newer query';
    await handler.whenCalled('search');
    await new Promise(resolve => setTimeout(resolve, 100));
    element.searchQuery = 'some even newer query';
    await handler.whenCalled('search');
    assertEquals(1, handler.getCallCount('sendQualityLog'));
  });

  test('SendsQualityLogOnDisconnect', async () => {
    element.remove();
    const clickedIndices = await handler.whenCalled('sendQualityLog');
    assertDeepEquals([], clickedIndices);
  });

  test('SendsQualityLogOnBeforeUnload', async () => {
    window.dispatchEvent(new Event('beforeunload'));
    const clickedIndices = await handler.whenCalled('sendQualityLog');
    assertDeepEquals([], clickedIndices);
  });

  test('ForceFlushesQualityLogOnBeforeUnload', async () => {
    handler.resetResolver('sendQualityLog');
    // Make the min age really long so we can test a beforeunload happening
    // before results are considered 'stable'.
    element.overrideQueryResultMinAgeForTesting(100000);

    window.dispatchEvent(new Event('beforeunload'));

    // Log should immediately be sent without having to wait the 100000ms.
    assertEquals(1, handler.getCallCount('sendQualityLog'));
  });

  test('SendsQualityLogOnlyOnce', async () => {
    // Click on a couple of the results.
    const resultsElements =
        element.shadowRoot!.querySelectorAll('cr-url-list-item');
    resultsElements[0]!.click();
    resultsElements[1]!.click();

    // Multiple events that can cause logs.
    element.searchQuery = 'some newer query';
    await handler.whenCalled('search');
    window.dispatchEvent(new Event('beforeunload'));
    element.remove();

    const clickedIndices = await handler.whenCalled('sendQualityLog');
    assertDeepEquals([0, 1], clickedIndices);
    assertEquals(1, handler.getCallCount('sendQualityLog'));
  });
});
