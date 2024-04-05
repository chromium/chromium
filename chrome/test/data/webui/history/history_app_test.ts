// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserServiceImpl} from 'chrome://history/history.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBrowserService} from './test_browser_service.js';

suite('HistoryAppTest', function() {
  let element: HistoryAppElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserServiceImpl.setInstance(new TestBrowserService());
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
});
