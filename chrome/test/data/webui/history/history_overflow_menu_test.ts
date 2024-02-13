// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionMenuModel, CrActionMenuElement, HistoryListElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, ensureLazyLoaded} from 'chrome://history/history.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry} from './test_util.js';

suite('#overflow-menu', function() {
  let listContainer: HistoryListElement;
  let sharedMenu: CrActionMenuElement;

  let target1: HTMLElement;
  let target2: HTMLElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);

    const app = document.createElement('history-app');
    document.body.appendChild(app);
    return Promise
        .all([
          testService.whenCalled('queryHistory'),
          ensureLazyLoaded(),
        ])
        .then(function() {
          listContainer = app.$.history;
          target1 = document.createElement('div');
          target2 = document.createElement('div');
          document.body.appendChild(target1);
          document.body.appendChild(target2);
          sharedMenu = listContainer.$.sharedMenu.get();
        });
  });

  test('opening and closing menu', async function() {
    const detail1: ActionMenuModel = {
      index: 1,
      item: createHistoryEntry(0, 'https://www.chromium.org'),
      target: target1,
    };
    listContainer.dispatchEvent(new CustomEvent(
        'open-menu', {bubbles: true, composed: true, detail: detail1}));
    assertTrue(sharedMenu.open);

    const moreButton = sharedMenu.querySelector<HTMLElement>('#menuMoreButton');
    assertTrue(!!moreButton);

    // Ensure that the menu corresponds to the clicked item.
    let whenChangeQueryFired = eventToPromise('change-query', listContainer);
    moreButton.click();
    let e = await whenChangeQueryFired;

    assertEquals('host:www.chromium.org', e.detail.search);

    sharedMenu.close();
    assertFalse(sharedMenu.open);

    // Open the menu for a different item.
    const detail2: ActionMenuModel = {
      index: 2,
      item: createHistoryEntry(0, 'https://www.wikipedia.org'),
      target: target2,
    };
    listContainer.dispatchEvent(new CustomEvent(
        'open-menu', {bubbles: true, composed: true, detail: detail2}));
    assertTrue(sharedMenu.open);

    // Ensure that the menu corresponds to the newly clicked item.
    whenChangeQueryFired = eventToPromise('change-query', listContainer);
    moreButton.click();
    e = await whenChangeQueryFired;
    assertEquals('host:www.wikipedia.org', e.detail.search);

    sharedMenu.close();
    assertFalse(sharedMenu.open);
  });
});
