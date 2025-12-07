// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ForeignSession, HistorySyncedDeviceCardElement, HistorySyncedDeviceManagerElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, HistorySignInState} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

// <if expr="not is_chromeos">
import { isChildVisible } from 'chrome://webui-test/test_util.js';
// </if>

import {TestBrowserService} from './test_browser_service.js';
import {createSession, createWindow} from './test_util.js';

function getCards(manager: HistorySyncedDeviceManagerElement):
    NodeListOf<HistorySyncedDeviceCardElement> {
  return manager.shadowRoot.querySelectorAll('history-synced-device-card');
}

function numWindowSeparators(card: HistorySyncedDeviceCardElement): number {
  return card.shadowRoot.querySelectorAll(':not([hidden]).window-separator')
      .length;
}

function assertNoSyncedTabsMessageShown(
    manager: HistorySyncedDeviceManagerElement, stringID: string) {
  assertFalse(manager.$['no-synced-tabs'].hidden);
  const message = loadTimeData.getString(stringID);
  assertNotEquals(-1, manager.$['no-synced-tabs'].textContent.indexOf(message));
}

suite('<history-synced-device-manager>', function() {
  let element: HistorySyncedDeviceManagerElement;
  let testService: TestBrowserService;

  function setForeignSessions(sessions: ForeignSession[]) {
    element.sessionList = sessions;
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState({}, '', '/');
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);

    element = document.createElement('history-synced-device-manager');
    // |signInState| is generally set after |searchTerm| in Polymer 2. Set in
    // the same order in tests, in order to catch regressions like
    // https://crbug.com/915641.
    element.searchTerm = '';
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_IN_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });
    document.body.appendChild(element);
  });

  test('single card, single window', async () => {
    const sessionList: ForeignSession[] = [createSession(
        'Nexus 5',
        [createWindow(['http://www.google.com', 'http://example.com'])])];
    setForeignSessions(sessionList);
    await microtasksFinished();

    const card =
        element.shadowRoot.querySelector('history-synced-device-card')!;
    assertEquals(
        'http://www.google.com',
        card.shadowRoot.querySelectorAll<HTMLElement>(
                           '.website-title')[0]!.textContent.trim());
    assertEquals(2, card.tabs.length);
  });

  test('two cards, multiple windows', async () => {
    const sessionList: ForeignSession[] = [
      createSession(
          'Nexus 5',
          [createWindow(['http://www.google.com', 'http://example.com'])]),
      createSession(
          'Nexus 6',
          [
            createWindow(['http://test.com']),
            createWindow(['http://www.gmail.com', 'http://badssl.com']),
          ]),
    ];
    setForeignSessions(sessionList);
    await microtasksFinished();

    const cards = getCards(element);
    assertEquals(2, cards.length);

    // Ensure separators between windows are added appropriately.
    assertEquals(0, numWindowSeparators(cards[0]!));
    assertEquals(1, numWindowSeparators(cards[1]!));
  });

  test('updating sessions', async () => {
    const session1 = createSession(
        'Chromebook',
        [createWindow(['http://www.example.com', 'http://crbug.com'])]);
    session1.timestamp = 1000;

    const session2 =
        createSession('Nexus 5', [createWindow(['http://www.google.com'])]);

    setForeignSessions([session1, session2]);
    await microtasksFinished();
    const session1updated = createSession('Chromebook', [
      createWindow(['http://www.example.com', 'http://crbug.com/new']),
      createWindow(['http://web.site']),
    ]);
    session1updated.timestamp = 1234;

    setForeignSessions([session1updated, session2]);
    await microtasksFinished();
    // There should only be two cards.
    const cards = getCards(element);
    assertEquals(2, cards.length);

    // There are now 2 windows in the first device.
    assertEquals(1, numWindowSeparators(cards[0]!));
    // Check that the actual link changes.
    assertEquals(
        'http://crbug.com/new',
        cards[0]!.shadowRoot.querySelectorAll<HTMLElement>(
                                '.website-title')[1]!.textContent.trim());
  });

  test('two cards, multiple windows, search', async () => {
    const sessionList: ForeignSession[] = [
      createSession(
          'Nexus 5',
          [createWindow(['http://www.google.com', 'http://example.com'])]),
      createSession(
          'Nexus 6',
          [
            createWindow(['http://www.gmail.com', 'http://badssl.com']),
            createWindow(['http://test.com']),
            createWindow(['http://www.gmail.com', 'http://bagssl.com']),
          ]),
    ];
    setForeignSessions(sessionList);
    await microtasksFinished();

    let cards = getCards(element);
    assertEquals(2, cards.length);

    // Ensure separators between windows are added appropriately.
    assertEquals(0, numWindowSeparators(cards[0]!));
    assertEquals(2, numWindowSeparators(cards[1]!));
    element.searchTerm = 'g';
    await microtasksFinished();

    cards = getCards(element);
    assertEquals(0, numWindowSeparators(cards[0]!));
    assertEquals(1, cards[0]!.tabs.length);
    assertEquals('http://www.google.com', cards[0]!.tabs[0]!.title);
    assertEquals(1, numWindowSeparators(cards[1]!));
    assertEquals(3, cards[1]!.tabs.length);
    assertEquals('http://www.gmail.com', cards[1]!.tabs[0]!.title);
    assertEquals('http://www.gmail.com', cards[1]!.tabs[1]!.title);
    assertEquals('http://bagssl.com', cards[1]!.tabs[2]!.title);

    // Ensure the title text is rendered during searches.
    assertEquals(
        'http://www.google.com',
        cards[0]!.shadowRoot.querySelectorAll<HTMLElement>(
                                '.website-title')[0]!.textContent.trim());

    element.searchTerm = 'Sans';
    await microtasksFinished();

    assertEquals(0, getCards(element).length);
    assertNoSyncedTabsMessageShown(element, 'noSearchResults');
  });

  test('delete a session', async () => {
    const sessionList: ForeignSession[] = [
      createSession('Nexus 5', [createWindow(['http://www.example.com'])]),
      createSession('Pixel C', [createWindow(['http://www.badssl.com'])]),
    ];

    setForeignSessions(sessionList);
    await microtasksFinished();

    let cards = getCards(element);
    assertEquals(2, cards.length);
    cards[0]!.$['menu-button'].click();
    await microtasksFinished();

    element.shadowRoot.querySelector<HTMLElement>('#menuDeleteButton')!.click();
    const deletedSessionTag =
        await testService.whenCalled('deleteForeignSession');
    assertEquals('Nexus 5', deletedSessionTag);

    // Simulate deleting the first device.
    setForeignSessions([sessionList[1]!]);
    await microtasksFinished();
    cards = getCards(element);
    assertEquals(1, cards.length);
    assertEquals('http://www.badssl.com', cards[0]!.tabs[0]!.title);
  });

  test('delete a collapsed session', async () => {
    const sessionList: ForeignSession[] = [
      createSession('Nexus 5', [createWindow(['http://www.example.com'])]),
      createSession('Pixel C', [createWindow(['http://www.badssl.com'])]),
    ];

    setForeignSessions(sessionList);
    await microtasksFinished();

    let cards = getCards(element);
    cards[0]!.$['card-heading'].click();
    await microtasksFinished();
    assertFalse(cards[0]!.opened);

    // Simulate deleting the first device.
    setForeignSessions([sessionList[1]!]);
    await microtasksFinished();
    cards = getCards(element);
    assertTrue(cards[0]!.opened);
  });

  test('click synced tab', async () => {
    setForeignSessions(
        [createSession('Chromebook', [createWindow(['https://example.com'])])]);
    await microtasksFinished();
    const cards = getCards(element);
    const anchor = cards[0]!.shadowRoot.querySelector('a')!;
    anchor.click();
    const args = await testService.whenCalled('openForeignSessionTab');
    assertEquals('Chromebook', args.sessionTag, 'sessionTag is correct');
    assertEquals(456, args.tabId, 'tabId is correct');
    assertFalse(args.e.altKey, 'altKey is defined');
    assertFalse(args.e.ctrlKey, 'ctrlKey is defined');
    assertFalse(args.e.metaKey, 'metaKey is defined');
    assertFalse(args.e.shiftKey, 'shiftKey is defined');
  });

  test('show actions menu', async () => {
    setForeignSessions(
        [createSession('Chromebook', [createWindow(['https://example.com'])])]);
    await microtasksFinished();
    const cards = getCards(element);
    cards[0]!.$['menu-button'].click();
    assertTrue(element.$.menu.getIfExists()!.open);
  });

  // <if expr="is_chromeos">
  // On ChromeOS only, the kReplaceSyncPromosWithSignInPromos flag is false and
  // this promo may be shown. For other platforms, the flag is true so this
  // promo is not shown (checked in other tests below).
  test('show sign in promo', async () => {
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_OUT,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();
    assertFalse(element.$['sign-in-guide'].hidden);
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_IN_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();
    assertTrue(element.$['sign-in-guide'].hidden);
  });
  // </if>

  test('no synced tabs message', async () => {
    // When user is not logged in, there is no synced tabs.
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_OUT,
      signInAllowed: true,
      guestSession: false,
    });
    element.clearSyncedDevicesForTest();
    await microtasksFinished();
    assertTrue(element.$['no-synced-tabs'].hidden);

    let cards = getCards(element);
    assertEquals(0, cards.length);

    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_IN_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });

    await microtasksFinished();
    // When user signs in, first show loading message.
    assertNoSyncedTabsMessageShown(element, 'loading');

    setForeignSessions([]);
    await microtasksFinished();
    cards = getCards(element);
    assertEquals(0, cards.length);
    // If no synced tabs are fetched, show 'no synced tabs'.
    assertNoSyncedTabsMessageShown(element, 'noSyncedResults');

    setForeignSessions([createSession(
        'Nexus 5',
        [createWindow(['http://www.google.com', 'http://example.com'])])]);

    await microtasksFinished();
    cards = getCards(element);
    assertEquals(1, cards.length);
    // If there are any synced tabs, hide the 'no synced tabs' message.
    assertTrue(element.$['no-synced-tabs'].hidden);

    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_OUT,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();
    // When user signs out, don't show the message.
    assertTrue(element.$['no-synced-tabs'].hidden);
  });

  test('hide sign in promo in guest mode', async () => {
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_OUT,
      signInAllowed: true,
      guestSession: true,
    });
    await microtasksFinished();
    assertTrue(element.$['sign-in-guide'].hidden);
  });

  test('hide sign-in promo if sign-in is disabled', async function() {
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_OUT,
      signInAllowed: false,
      guestSession: false,
    });
    await microtasksFinished();
    assertTrue(element.$['sign-in-guide'].hidden);
  });

  test('no synced tabs message displays on load', async () => {
    element.clearSyncedDevicesForTest();
    // Should show no synced tabs message on initial load. Regression test for
    // https://crbug.com/915641.
    await microtasksFinished();
    assertNoSyncedTabsMessageShown(element, 'noSyncedResults');
    const cards = getCards(element);
    assertEquals(0, cards.length);
  });
});

// <if expr="not is_chromeos">
// history-sync-optin elements is not shown for ChromeOS.
suite('<history-sync-optin>', function() {
  let element: HistorySyncedDeviceManagerElement;
  let testService: TestBrowserService;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState({}, '', '/');
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);

    // history-sync-optin elements are only shown when the
    // replaceSyncPromosWithSignInPromos is true
    loadTimeData.overrideValues({
      replaceSyncPromosWithSignInPromos: true,
    });

    element = document.createElement('history-synced-device-manager');
    // |signInState| is generally set after |searchTerm| in Polymer 2. Set in
    // the same order in tests, in order to catch regressions like
    // https://crbug.com/915641.
    element.searchTerm = '';
    element.configureSignInForTest({
      // Setting the sign in state to WEB_ONLY_SIGNED_IN, because user's name,
      // email and profile icon are only shown on the page when the sign in
      // state is WEB_ONLY_SIGNED_IN.
      signInState: HistorySignInState.WEB_ONLY_SIGNED_IN,
      signInAllowed: true,
      guestSession: false,
    });
    document.body.appendChild(element);
  });

  test('check history sync optin elements are visible', async () => {
    await microtasksFinished();

    // The old promo should not be visible.
    assertFalse(isChildVisible(element, '#turn-on-sync-promo'));

    // The new promo elements for WEB_ONLY_SIGNED_IN state are shown correctly.
    assertTrue(isChildVisible(element, '#sync-history-button'));
    assertTrue(isChildVisible(element, '#history-sync-optin'));
    assertTrue(isChildVisible(element, '#account-name'));
    assertTrue(isChildVisible(element, '#account-email'));
    assertTrue(isChildVisible(element, '#profile-icon'));
  });

   test('check elements in signed out state', async () => {
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_OUT,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();
    // Should not be visible with kReplaceSyncPromosWithSignInPromos enabled.
    assertFalse(isChildVisible(element, '#sign-in-guide'));
    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(element, '#signed-in-sync-history-promo-desc'));
    assertFalse(isChildVisible(element, '#verify-its-you-button'));
    assertFalse(isChildVisible(
        element, '#sign-in-pending-sync-history-promo-desc-sync-history-on'));
    assertFalse(isChildVisible(element, '#sign-in-pending-avatar'));

    // The history sync promo elements for SIGNED_OUT state are shown correctly.
    assertTrue(isChildVisible(element, '#sync-history-button'));
    assertTrue(isChildVisible(element, '#signed-out-sync-history-promo-desc'));
  });

  test('check elements in pending signin without tabs sync state', async () => {
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGN_IN_PENDING_NOT_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(element, '#signed-out-sync-history-promo-desc'));
    assertFalse(isChildVisible(element, '#signed-in-sync-history-promo-desc'));
    assertFalse(isChildVisible(element, '#verify-its-you-button'));
    assertFalse(isChildVisible(
        element, '#sign-in-pending-sync-history-promo-desc-sync-history-on'));

    // The promo elements for SIGN_IN_PENDING_NOT_SYNCING_TABS state are
    // shown correctly.
    assertTrue(isChildVisible(element, '#sign-in-pending-avatar'));
    assertTrue(
        isChildVisible(element, '#sign-in-pending-sync-history-promo-desc'));
    assertTrue(isChildVisible(element, '#sync-history-button'));
  });

  test('check elements in pending signin with tabs sync state', async () => {
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGN_IN_PENDING_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();

    // The other states promo elements should not be visible.
    assertFalse(isChildVisible(element, '#sync-history-button'));
    assertFalse(
        isChildVisible(element, '#sign-in-pending-sync-history-promo-desc'));

    // The promo elements for SIGN_IN_PENDING_SYNCING_TABS state are
    // shown correctly.
    assertTrue(isChildVisible(element, '#sign-in-pending-avatar'));
    assertTrue(isChildVisible(
        element, '#sign-in-pending-sync-history-promo-desc-sync-history-on'));
    assertTrue(isChildVisible(element, '#verify-its-you-button'));
  });

  test('check elements in SYNC_DISABLED sign in state', async () => {
    element.configureSignInForTest({
      signInState: HistorySignInState.SYNC_DISABLED,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();

    // The 'no synced tabs' message should be shown.
    assertTrue(isChildVisible(element, '#no-synced-tabs'));

    // The promo elements are not shown
    assertFalse(isChildVisible(element, '#history-sync-optin'));
    assertFalse(isChildVisible(element, '#sync-history-button'));
    assertFalse(isChildVisible(element, '#verify-its-you-button'));
  });

  test('initializes account info', async () => {
    await testService.handler.whenCalled('requestAccountInfo');
    await microtasksFinished();

    assertEquals(
        'Test User',
        element.shadowRoot.querySelector<HTMLElement>(
                              '#account-name')!.textContent.trim());
    assertEquals(
        'test@google.com',
        element.shadowRoot.querySelector<HTMLElement>(
                              '#account-email')!.textContent.trim());
    assertEquals(
        'http://example.com/image.png',
        element.shadowRoot.querySelector<HTMLImageElement>(
                              '#profile-icon')!.src);
  });

  test('updates account info', async () => {
    await testService.handler.whenCalled('requestAccountInfo');
    await microtasksFinished();

    const newAccountInfo = {
      name: 'Test User 2',
      email: 'test2@google.com',
      accountImageSrc: {url: 'http://image.com/img2.png'},
    };

    testService.pageRemote.sendAccountInfo(newAccountInfo);
    await microtasksFinished();

    assertEquals(
        newAccountInfo.name,
        element.shadowRoot.querySelector<HTMLElement>(
                              '#account-name')!.textContent.trim());
    assertEquals(
        newAccountInfo.email,
        element.shadowRoot.querySelector<HTMLElement>(
                              '#account-email')!.textContent.trim());
    assertEquals(
        newAccountInfo.accountImageSrc.url,
        element.shadowRoot.querySelector<HTMLImageElement>(
                              '#profile-icon')!.src);
  });

  test('calls turnOnHistorySync when button is clicked', async () => {
    element.configureSignInForTest({
      // For the sake of diversity, using other signin state in this test,
      // different from WEB_ONLY_SIGN_IN
      signInState: HistorySignInState.SIGNED_IN_NOT_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });
    const button =
        element.shadowRoot.querySelector<HTMLElement>('#sync-history-button');
    assertTrue(!!button);
    button.click();

    await testService.handler.whenCalled('turnOnHistorySync');
  });

  test('check recorded metrics in pending signin', async () => {
    // The signin pending offered histogram is recorded once.
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGN_IN_PENDING_NOT_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();
    assertEquals(1, testService.getCallCount('recordSigninPendingOffered'));

    // Firing a sign in pending state again does not record again.
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGN_IN_PENDING_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();
    assertEquals(1, testService.getCallCount('recordSigninPendingOffered'));

    // Signing in and then getting into sign in pending state again records the
    // histogram again.
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGNED_IN_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();
    element.configureSignInForTest({
      signInState: HistorySignInState.SIGN_IN_PENDING_SYNCING_TABS,
      signInAllowed: true,
      guestSession: false,
    });
    await microtasksFinished();

    assertEquals(2, testService.getCallCount('recordSigninPendingOffered'));
  });
});
// </if>
