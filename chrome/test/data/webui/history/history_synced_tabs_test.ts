// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ForeignSession, HistorySyncedDeviceCardElement, HistorySyncedDeviceManagerElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, ensureLazyLoaded} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createSession, createWindow} from './test_util.js';

function getCards(manager: HistorySyncedDeviceManagerElement):
    NodeListOf<HistorySyncedDeviceCardElement> {
  return manager.shadowRoot!.querySelectorAll('history-synced-device-card');
}

function numWindowSeparators(card: HistorySyncedDeviceCardElement): number {
  return card.shadowRoot!.querySelectorAll(':not([hidden]).window-separator')
      .length;
}

function assertNoSyncedTabsMessageShown(
    manager: HistorySyncedDeviceManagerElement, stringID: string) {
  assertFalse(manager.$['no-synced-tabs'].hidden);
  const message = loadTimeData.getString(stringID);
  assertNotEquals(
      -1, manager.$['no-synced-tabs'].textContent!.indexOf(message));
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

    // Need to ensure lazy_load.html has been imported so that the device
    // manager custom element is defined.
    return ensureLazyLoaded().then(() => {
      element = document.createElement('history-synced-device-manager');
      // |signInState| is generally set after |searchTerm| in Polymer 2. Set in
      // the same order in tests, in order to catch regressions like
      // https://crbug.com/915641.
      element.searchTerm = '';
      element.configureSignInForTest(
          {signInState: true, signInAllowed: true, guestSession: false});
      document.body.appendChild(element);
    });
  });

  test('single card, single window', function() {
    const sessionList: ForeignSession[] = [createSession(
        'Nexus 5',
        [createWindow(['http://www.google.com', 'http://example.com'])])];
    setForeignSessions(sessionList);

    return flushTasks().then(function() {
      const card =
          element.shadowRoot!.querySelector('history-synced-device-card')!;
      assertEquals(
          'http://www.google.com',
          card.shadowRoot!.querySelectorAll<HTMLElement>(
                              '.website-title')[0]!.textContent!.trim());
      assertEquals(2, card.tabs.length);
    });
  });

  test('two cards, multiple windows', function() {
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

    return flushTasks().then(function() {
      const cards = getCards(element);
      assertEquals(2, cards.length);

      // Ensure separators between windows are added appropriately.
      assertEquals(0, numWindowSeparators(cards[0]!));
      assertEquals(1, numWindowSeparators(cards[1]!));
    });
  });

  test('updating sessions', function() {
    const session1 = createSession(
        'Chromebook',
        [createWindow(['http://www.example.com', 'http://crbug.com'])]);
    session1.timestamp = 1000;

    const session2 =
        createSession('Nexus 5', [createWindow(['http://www.google.com'])]);

    setForeignSessions([session1, session2]);

    return flushTasks()
        .then(function() {
          const session1updated = createSession('Chromebook', [
            createWindow(['http://www.example.com', 'http://crbug.com/new']),
            createWindow(['http://web.site']),
          ]);
          session1updated.timestamp = 1234;

          setForeignSessions([session1updated, session2]);

          return flushTasks();
        })
        .then(function() {
          // There should only be two cards.
          const cards = getCards(element);
          assertEquals(2, cards.length);

          // There are now 2 windows in the first device.
          assertEquals(1, numWindowSeparators(cards[0]!));

          // Check that the actual link changes.
          assertEquals(
              'http://crbug.com/new',
              cards[0]!.shadowRoot!
                  .querySelectorAll<HTMLElement>(
                      '.website-title')[1]!.textContent!.trim());
        });
  });

  test('two cards, multiple windows, search', function() {
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

    return flushTasks()
        .then(function() {
          const cards = getCards(element);
          assertEquals(2, cards.length);

          // Ensure separators between windows are added appropriately.
          assertEquals(0, numWindowSeparators(cards[0]!));
          assertEquals(2, numWindowSeparators(cards[1]!));
          element.searchTerm = 'g';

          return flushTasks();
        })
        .then(function() {
          const cards = getCards(element);

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
              cards[0]!.shadowRoot!
                  .querySelectorAll<HTMLElement>(
                      '.website-title')[0]!.textContent!.trim());

          element.searchTerm = 'Sans';
          return flushTasks();
        })
        .then(function() {
          assertEquals(0, getCards(element).length);

          assertNoSyncedTabsMessageShown(element, 'noSearchResults');
        });
  });

  test('delete a session', function() {
    const sessionList: ForeignSession[] = [
      createSession('Nexus 5', [createWindow(['http://www.example.com'])]),
      createSession('Pixel C', [createWindow(['http://www.badssl.com'])]),
    ];

    setForeignSessions(sessionList);

    return flushTasks()
        .then(function() {
          const cards = getCards(element);
          assertEquals(2, cards.length);

          cards[0]!.$['menu-button'].click();
          return flushTasks();
        })
        .then(function() {
          element.shadowRoot!.querySelector<HTMLElement>(
                                 '#menuDeleteButton')!.click();
          return testService.whenCalled('deleteForeignSession');
        })
        .then(args => {
          assertEquals('Nexus 5', args);

          // Simulate deleting the first device.
          setForeignSessions([sessionList[1]!]);

          return flushTasks();
        })
        .then(function() {
          const cards = getCards(element);
          assertEquals(1, cards.length);
          assertEquals('http://www.badssl.com', cards[0]!.tabs[0]!.title);
        });
  });

  test('delete a collapsed session', async () => {
    const sessionList: ForeignSession[] = [
      createSession('Nexus 5', [createWindow(['http://www.example.com'])]),
      createSession('Pixel C', [createWindow(['http://www.badssl.com'])]),
    ];

    setForeignSessions(sessionList);
    await flushTasks();

    let cards = getCards(element);
    cards[0]!.$['card-heading'].click();
    await microtasksFinished();
    assertFalse(cards[0]!.opened);

    // Simulate deleting the first device.
    setForeignSessions([sessionList[1]!]);
    await flushTasks();
    cards = getCards(element);
    assertTrue(cards[0]!.opened);
  });

  test('click synced tab', function() {
    setForeignSessions(
        [createSession('Chromebook', [createWindow(['https://example.com'])])]);
    return flushTasks()
        .then(function() {
          const cards = getCards(element);
          const anchor = cards[0]!.shadowRoot!.querySelector('a')!;
          anchor.click();
          return testService.whenCalled('openForeignSessionTab');
        })
        .then(args => {
          assertEquals('Chromebook', args.sessionTag, 'sessionTag is correct');
          assertEquals(456, args.tabId, 'tabId is correct');
          assertFalse(args.e.altKey, 'altKey is defined');
          assertFalse(args.e.ctrlKey, 'ctrlKey is defined');
          assertFalse(args.e.metaKey, 'metaKey is defined');
          assertFalse(args.e.shiftKey, 'shiftKey is defined');
        });
  });

  test('show actions menu', function() {
    setForeignSessions(
        [createSession('Chromebook', [createWindow(['https://example.com'])])]);

    return flushTasks().then(function() {
      const cards = getCards(element);
      cards[0]!.$['menu-button'].click();
      assertTrue(element.$.menu.getIfExists()!.open);
    });
  });

  test('show sign in promo', function() {
    element.configureSignInForTest(
        {signInState: false, signInAllowed: true, guestSession: false});
    return flushTasks()
        .then(function() {
          assertFalse(element.$['sign-in-guide'].hidden);
          element.configureSignInForTest(
              {signInState: true, signInAllowed: true, guestSession: false});
          return flushTasks();
        })
        .then(function() {
          assertTrue(element.$['sign-in-guide'].hidden);
        });
  });

  test('no synced tabs message', function() {
    // When user is not logged in, there is no synced tabs.
    element.configureSignInForTest(
        {signInState: false, signInAllowed: true, guestSession: false});
    element.clearSyncedDevicesForTest();
    return flushTasks()
        .then(function() {
          assertTrue(element.$['no-synced-tabs'].hidden);

          const cards = getCards(element);
          assertEquals(0, cards.length);

          element.configureSignInForTest(
              {signInState: true, signInAllowed: true, guestSession: false});

          return flushTasks();
        })
        .then(function() {
          // When user signs in, first show loading message.
          assertNoSyncedTabsMessageShown(element, 'loading');

          const sessionList: ForeignSession[] = [];
          setForeignSessions(sessionList);
          return flushTasks();
        })
        .then(function() {
          const cards = getCards(element);
          assertEquals(0, cards.length);
          // If no synced tabs are fetched, show 'no synced tabs'.
          assertNoSyncedTabsMessageShown(element, 'noSyncedResults');

          const sessionList: ForeignSession[] = [createSession(
              'Nexus 5',
              [createWindow(['http://www.google.com', 'http://example.com'])])];
          setForeignSessions(sessionList);

          return flushTasks();
        })
        .then(function() {
          const cards = getCards(element);
          assertEquals(1, cards.length);
          // If there are any synced tabs, hide the 'no synced tabs' message.
          assertTrue(element.$['no-synced-tabs'].hidden);

          element.configureSignInForTest(
              {signInState: false, signInAllowed: true, guestSession: false});
          return flushTasks();
        })
        .then(function() {
          // When user signs out, don't show the message.
          assertTrue(element.$['no-synced-tabs'].hidden);
        });
  });

  test('hide sign in promo in guest mode', function() {
    element.configureSignInForTest(
        {signInState: false, signInAllowed: true, guestSession: true});
    return flushTasks().then(function() {
      assertTrue(element.$['sign-in-guide'].hidden);
    });
  });

  test('hide sign-in promo if sign-in is disabled', async function() {
    element.configureSignInForTest(
        {signInState: false, signInAllowed: false, guestSession: false});
    await flushTasks();
    assertTrue(element.$['sign-in-guide'].hidden);
  });

  test('no synced tabs message displays on load', function() {
    element.clearSyncedDevicesForTest();
    // Should show no synced tabs message on initial load. Regression test for
    // https://crbug.com/915641.
    return Promise.all([flushTasks(), waitBeforeNextRender(element)])
        .then(() => {
          assertNoSyncedTabsMessageShown(element, 'noSyncedResults');
          const cards = getCards(element);
          assertEquals(0, cards.length);
        });
  });
});
