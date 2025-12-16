// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, CrRouter, HistoryEmbeddingsBrowserProxyImpl, HistoryEmbeddingsPageHandlerRemote} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

// <if expr="not is_chromeos">
import { isChildVisible } from 'chrome://webui-test/test_util.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {HistorySignInState, SyncState} from 'chrome://history/history.js';
// </if>

import {TestBrowserService} from './test_browser_service.js';

suite('HistoryAppTest', function() {
  let element: HistoryAppElement;
  let browserService: TestBrowserService;
  let embeddingsHandler: TestMock<HistoryEmbeddingsPageHandlerRemote>&
      HistoryEmbeddingsPageHandlerRemote;

  // Force cr-history-embeddings to be in the DOM for testing.
  function forceHistoryEmbeddingsElement() {
    loadTimeData.overrideValues({historyEmbeddingsSearchMinimumWordCount: 0});
    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'some fake input'}}));
    return microtasksFinished();
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
    return microtasksFinished();
  });

  test('SetsScrollTarget', async () => {
    assertEquals(
        element.$.tabsScrollContainer, element.getScrollTargetForTesting());

    // 'By group' view shares the same scroll container as default history view.
    element.$.router.selectedPage = 'grouped';
    await microtasksFinished();
    assertEquals(
        element.$.tabsScrollContainer, element.getScrollTargetForTesting());

    // Switching to synced tabs should change scroll target to it.
    element.$.router.selectedPage = 'syncedTabs';
    await microtasksFinished();
    assertEquals(
        element.shadowRoot.querySelector('#syncedDevicesScroll'),
        element.getScrollTargetForTesting());
  });

  test('ShowsHistoryEmbeddings', async () => {
    // By default, embeddings should not even be in the DOM.
    assertFalse(!!element.shadowRoot.querySelector('cr-history-embeddings'));

    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'one'}}));
    await microtasksFinished();
    assertFalse(!!element.shadowRoot.querySelector('cr-history-embeddings'));

    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'two words'}}));
    await microtasksFinished();
    assertTrue(!!element.shadowRoot.querySelector('cr-history-embeddings'));

    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'one'}}));
    await microtasksFinished();
    assertFalse(!!element.shadowRoot.querySelector('cr-history-embeddings'));
  });

  test('SetsScrollOffset', async () => {
    function resizeAndWait(height: number) {
      const historyEmbeddingsContainer =
          element.shadowRoot.querySelector<HTMLElement>(
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
    await microtasksFinished();
    assertEquals(700, element.$.history.scrollOffset);

    await resizeAndWait(400);
    await microtasksFinished();
    assertEquals(400, element.$.history.scrollOffset);
  });

  test('QueriesMoreFromSiteFromHistoryEmbeddings', async () => {
    element.dispatchEvent(new CustomEvent(
        'change-query',
        {bubbles: true, composed: true, detail: {search: 'two words'}}));
    await microtasksFinished();
    const historyEmbeddings =
        element.shadowRoot.querySelector('cr-history-embeddings');
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
    await microtasksFinished();
    const historyEmbeddings =
        element.shadowRoot.querySelector('cr-history-embeddings');
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
    const removeVisitsArg =
        await browserService.handler.whenCalled('removeVisits');
    assertEquals(1, removeVisitsArg.length);
    assertEquals('http://google.com', removeVisitsArg[0].url);
    assertEquals(1, removeVisitsArg[0].timestamps.length);
    assertEquals(1000, removeVisitsArg[0].timestamps[0]);
  });

  test('ChangesQueryStateWithFilterChips', async () => {
    const filterChips =
        element.shadowRoot.querySelector('cr-history-embeddings-filter-chips')!;
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
    await microtasksFinished();

    const expectedDateObject = new Date('2022-04-02T00:00:00');

    const filterChips =
        element.shadowRoot.querySelector('cr-history-embeddings-filter-chips')!;
    assertTrue(!!filterChips);
    assertEquals(
        expectedDateObject.getTime(), filterChips.timeRangeStart?.getTime());

    const historyEmbeddings =
        element.shadowRoot.querySelector('cr-history-embeddings');
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
    await microtasksFinished();
    assertEquals(timeRangeStartObj, historyEmbeddings.timeRangeStart);

    // Clear the after date query.
    element.dispatchEvent(new CustomEvent('change-query', {
      bubbles: true,
      composed: true,
      detail: {
        search: 'two words',
      },
    }));
    await microtasksFinished();
    assertEquals(null, historyEmbeddings.timeRangeStart);
  });

  test('UsesMinWordCount', async () => {
    loadTimeData.overrideValues({historyEmbeddingsSearchMinimumWordCount: 4});
    element.dispatchEvent(new CustomEvent('change-query', {
      bubbles: true,
      composed: true,
      detail: {search: 'two words'},
    }));
    await microtasksFinished();

    let historyEmbeddings =
        element.shadowRoot.querySelector('cr-history-embeddings');
    assertFalse(!!historyEmbeddings);

    element.dispatchEvent(new CustomEvent('change-query', {
      bubbles: true,
      composed: true,
      detail: {search: 'at least four words'},
    }));
    await microtasksFinished();
    historyEmbeddings =
        element.shadowRoot.querySelector('cr-history-embeddings');
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
      return microtasksFinished();
    }

    function getCount() {
      const historyEmbeddingsElement =
          element.shadowRoot.querySelector('cr-history-embeddings')!;
      return historyEmbeddingsElement.numCharsForQuery;
    }

    await dispatchNativeInput({data: 'a'}, 'a');
    assertEquals(1, getCount(), 'counts normal characters');
    await dispatchNativeInput({data: 'b'}, 'ab');
    await dispatchNativeInput({data: 'c'}, 'abc');
    assertEquals(3, getCount(), 'counts additional characters');

    await dispatchNativeInput({data: 'pasted text'}, 'pasted text');
    assertEquals(1, getCount(), 'insert that replaces all text counts as 1');

    await dispatchNativeInput({data: 'more text'}, 'pasted text more text');
    assertEquals(
        2, getCount(), 'insert that adds to existing input increments count');

    await dispatchNativeInput({data: null}, 'pasted text more tex');
    assertEquals(3, getCount(), 'deletion increments');

    await dispatchNativeInput({data: null}, '');
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
    await microtasksFinished();
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

  test('PassesDisclaimerLinkClicksToEmbeddings', async () => {
    await forceHistoryEmbeddingsElement();
    const historyEmbeddingsElement =
        element.shadowRoot.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddingsElement);
    assertFalse(historyEmbeddingsElement.forceSuppressLogging);
    element.$.historyEmbeddingsDisclaimerLink.click();
    await microtasksFinished();
    assertTrue(historyEmbeddingsElement.forceSuppressLogging);
  });

  test('PassesDisclaimerLinkAuxClicksToEmbeddings', async () => {
    await forceHistoryEmbeddingsElement();
    const historyEmbeddingsElement =
        element.shadowRoot.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddingsElement);
    assertFalse(historyEmbeddingsElement.forceSuppressLogging);
    element.$.historyEmbeddingsDisclaimerLink.dispatchEvent(
        new MouseEvent('auxclick'));
    await microtasksFinished();
    assertTrue(historyEmbeddingsElement.forceSuppressLogging);
  });

  test('SetsDateTimeFormatForEmbeddings', async () => {
    await forceHistoryEmbeddingsElement();
    const historyEmbeddingsElement =
        element.shadowRoot.querySelector('cr-history-embeddings');
    assertTrue(!!historyEmbeddingsElement);
    assertFalse(historyEmbeddingsElement.showRelativeTimes);

    element.$.router.selectedPage = 'grouped';
    await microtasksFinished();
    assertTrue(historyEmbeddingsElement.showRelativeTimes);

    element.$.router.selectedPage = 'history';
    await microtasksFinished();
    assertFalse(historyEmbeddingsElement.showRelativeTimes);
  });
});

// <if expr="not is_chromeos">
// history sync promo is not shown for ChromeOS.
suite('HistoryAppUnoPhase2FollowUpTest', () => {
  let element: HistoryAppElement;
  let browserService: TestBrowserService;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      unoPhase2FollowUp: true,
    });
    browserService = new TestBrowserService();
    BrowserServiceImpl.setInstance(browserService);
    browserService.handler.setResultFor(
        'shouldShowHistoryPageHistorySyncPromo', Promise.resolve({
          shouldShow: true,
        }));

    element = document.createElement('history-app');
    document.body.appendChild(element);
    return microtasksFinished();
  });

  test('ShowsHistorySyncPromoElementWhenDataIsTrue', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGNED_IN,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();

    assertTrue(
        isChildVisible(element, 'history-sync-promo'), 'Promo should be shown');
  });

  test('HidesHistorySyncPromoElementWhenDataIsFalse', async () => {
    browserService.handler.setResultFor(
        'shouldShowHistoryPageHistorySyncPromo',
        Promise.resolve({shouldShow: false}));
    // Re-create the element to pick up the new loadTimeData.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('history-app');
    document.body.appendChild(element);
    await microtasksFinished();

    assertFalse(
        isChildVisible(element, 'history-sync-promo'),
        'Promo should not be shown');
  });

  test('HidesHistorySyncPromoElementWhenHistorySyncIsDisabled', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGNED_IN,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.DISABLED,
    });
    await microtasksFinished();

    assertFalse(
        isChildVisible(element, 'history-sync-promo'),
        'Promo should not be shown');
  });

  test('HistorySyncPromoElementSignedIn', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGNED_IN,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertTrue(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-button'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#signed-out-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#verify-its-you-button'));
  });

  test('HistorySyncPromoElementPendingSignInWithHistorySyncOn', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGN_IN_PENDING,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_ON,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertTrue(isChildVisible(
        historySyncPromo, '#sign-in-pending-syncing-history-description'));
    assertTrue(isChildVisible(historySyncPromo, '#verify-its-you-button'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#signed-out-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertFalse(isChildVisible(historySyncPromo, '#sync-history-button'));
  });

  test('HistorySyncPromoElementPendingSignInWithHistorySyncOff', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGN_IN_PENDING,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertTrue(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-button'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#signed-out-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertFalse(isChildVisible(historySyncPromo, '#verify-its-you-button'));
  });

  test('HistorySyncPromoElementWebOnlySignIn', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.WEB_ONLY_SIGNED_IN,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(
        isChildVisible(historySyncPromo, '#web-only-signed-in-description'));
    assertTrue(isChildVisible(historySyncPromo, '#profile-info-row'));
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-button'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertFalse(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#verify-its-you-button'));
  });

  test('HistorySyncPromoElementSignedOut', async () => {
    webUIListenerCallback('history-identity-state-changed', {
      signIn: HistorySignInState.SIGNED_OUT,
      tabsSync: SyncState.TURNED_OFF,
      historySync: SyncState.TURNED_OFF,
    });
    await microtasksFinished();
    const historySyncPromo =
        element.shadowRoot.querySelector('history-sync-promo');
    assertTrue(!!historySyncPromo, 'Promo should be shown');

    // The promo elements for current state are shown correctly.
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-illustration'));
    assertTrue(isChildVisible(historySyncPromo, '#signed-out-description'));
    assertTrue(isChildVisible(historySyncPromo, '#sync-history-button'));

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(historySyncPromo, '#signed-in-description'));
    assertFalse(isChildVisible(
        historySyncPromo, '#sign-in-pending-not-syncing-history-description'));
    assertFalse(isChildVisible(historySyncPromo, '#verify-its-you-button'));
  });
});
// </if>
