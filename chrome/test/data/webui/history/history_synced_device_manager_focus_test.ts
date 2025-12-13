// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistorySyncedDeviceManagerElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, HistorySignInState, SyncState} from 'chrome://history/history.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createSession, createWindow} from './test_util.js';

suite('<history-synced-device-manager>', function() {
  let element: HistorySyncedDeviceManagerElement;
  let testService: TestBrowserService;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);
    testService.setInitialIdentityState({
      signIn: HistorySignInState.SIGNED_IN,
      tabsSync: SyncState.TURNED_ON,
      historySync: SyncState.TURNED_OFF,
    });
    element = document.createElement('history-synced-device-manager');
    element.searchTerm = '';
    document.body.appendChild(element);
  });

  test('focus and keyboard nav', async () => {
    async function waitForFocusGridUpdate() {
      await microtasksFinished();
      // Wait for debounced focus grid update.
      await new Promise(resolve => setTimeout(resolve));
    }

    const sessionList = [
      createSession(
          'Nexus 5',
          [createWindow(['http://www.example.com', 'http://www.google.com'])]),
      createSession('Pixel C', [createWindow(['http://www.badssl.com'])]),
      createSession('Potato', [createWindow(['http://www.wikipedia.org'])]),
    ];

    element.sessionList = sessionList;
    await waitForFocusGridUpdate();

    let cards =
        element.shadowRoot.querySelectorAll('history-synced-device-card');
    assertTrue(!!cards[0]);
    assertTrue(!!cards[1]);

    let focused = cards[0].$['menu-button'];
    focused.focus();

    // Go to the collapse button.
    pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
    focused = cards[0].$['collapse-button'];
    assertEquals(focused, getDeepActiveElement());

    // Go to the first url.
    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused =
        cards[0].shadowRoot.querySelectorAll<HTMLElement>('.website-link')[0]!;
    assertEquals(focused, getDeepActiveElement());

    // Collapse the first card.
    pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    focused = cards[0].$['collapse-button'];
    assertEquals(focused, getDeepActiveElement());
    focused.click();
    await waitForFocusGridUpdate();

    // Pressing down goes to the next card.
    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused = cards[1].$['collapse-button'];
    assertEquals(focused, getDeepActiveElement());

    // Expand the first card.
    pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    focused = cards[0].$['collapse-button'];
    assertEquals(focused, getDeepActiveElement());
    focused.click();
    await waitForFocusGridUpdate();

    // First card's urls are focusable again.
    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused =
        cards[0].shadowRoot.querySelectorAll<HTMLElement>('.website-link')[0]!;
    assertEquals(focused, getDeepActiveElement());

    // Remove the second URL from the first card.
    sessionList[0]!.windows[0]!.tabs.splice(1, 1);
    element.sessionList = sessionList.slice();
    await waitForFocusGridUpdate();

    cards = element.shadowRoot.querySelectorAll('history-synced-device-card');
    assertTrue(!!cards[0]);
    assertTrue(!!cards[1]);

    // Go to the next card's menu buttons.
    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused = cards[1].$['collapse-button'];
    assertEquals(focused, getDeepActiveElement());

    pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
    focused =
        cards[0].shadowRoot.querySelectorAll<HTMLElement>('.website-link')[0]!;
    assertEquals(focused, getDeepActiveElement());

    // Remove the second card.
    sessionList.splice(1, 1);
    element.sessionList = sessionList.slice();
    await waitForFocusGridUpdate();

    cards = element.shadowRoot.querySelectorAll('history-synced-device-card');
    assertTrue(!!cards[1]);

    // Pressing down goes to the next card.
    pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
    focused = cards[1].$['collapse-button'];
    assertEquals(focused, getDeepActiveElement());
  });
});
