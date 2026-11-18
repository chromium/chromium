// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionMenuModel, CrActionMenuElement, HistoryListElement} from 'chrome://history/history.js';
import {BrowserProxyImpl} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';
import {createHistoryEntry} from './test_util.js';

async function setupOverflowMenu(): Promise<{
  listContainer: HistoryListElement,
  sharedMenu: CrActionMenuElement,
  target1: HTMLElement,
  target2: HTMLElement,
}> {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const testProxy = new TestHistoryBrowserProxy();
  BrowserProxyImpl.setInstance(testProxy);

  const app = document.createElement('history-app');
  document.body.appendChild(app);
  await testProxy.handler.whenCalled('queryHistory');
  const listContainer = app.$.history;
  const target1 = document.createElement('div');
  const target2 = document.createElement('div');
  document.body.appendChild(target1);
  document.body.appendChild(target2);
  const sharedMenu = listContainer.$.sharedMenu.get();
  return {listContainer, sharedMenu, target1, target2};
}

suite('#overflow-menu', function() {
  let listContainer: HistoryListElement;
  let sharedMenu: CrActionMenuElement;

  let target1: HTMLElement;
  let target2: HTMLElement;

  setup(async function() {
    // TODO(crbug.com/584210521): Remove override once
    // kBrowsingHistoryFilterByDomain is launched.
    loadTimeData.overrideValues({
      isBrowsingHistoryFilterByDomainEnabled: true,
    });
    ({listContainer, sharedMenu, target1, target2} = await setupOverflowMenu());
  });

  test('opening and closing menu', async function() {
    const detail1: ActionMenuModel = {
      index: 1,
      item: createHistoryEntry(0, 'https://www.chromium.org'),
      target: target1,
    };
    listContainer.fire('open-menu', detail1);
    await microtasksFinished();
    assertTrue(sharedMenu.open);

    const moreButton = sharedMenu.querySelector<HTMLElement>('#menuMoreButton');
    assertTrue(!!moreButton);

    // Ensure that the menu corresponds to the clicked item.
    let whenDomainFilterSelected =
        eventToPromise('domain-filter-selected', listContainer);
    moreButton.click();
    let e = await whenDomainFilterSelected;

    assertEquals('www.chromium.org', e.detail.domain);

    sharedMenu.close();
    assertFalse(sharedMenu.open);

    // Open the menu for a different item.
    const detail2: ActionMenuModel = {
      index: 2,
      item: createHistoryEntry(0, 'https://www.wikipedia.org'),
      target: target2,
    };
    listContainer.fire('open-menu', detail2);
    await microtasksFinished();
    assertTrue(sharedMenu.open);

    // Ensure that the menu corresponds to the newly clicked item.
    whenDomainFilterSelected =
        eventToPromise('domain-filter-selected', listContainer);
    moreButton.click();
    e = await whenDomainFilterSelected;
    assertEquals('www.wikipedia.org', e.detail.domain);

    sharedMenu.close();
    assertFalse(sharedMenu.open);
  });
});

// TODO(crbug.com/584210521): Remove once kBrowsingHistoryFilterByDomain is
// launched and legacy "host:" prefix parsing is removed.
suite('#overflow-menu with domain filter disabled', function() {
  let listContainer: HistoryListElement;
  let sharedMenu: CrActionMenuElement;

  let target1: HTMLElement;
  let target2: HTMLElement;

  setup(async function() {
    loadTimeData.overrideValues({
      isBrowsingHistoryFilterByDomainEnabled: false,
    });
    ({listContainer, sharedMenu, target1, target2} = await setupOverflowMenu());
  });

  test('opening and closing menu', async function() {
    const detail1: ActionMenuModel = {
      index: 1,
      item: createHistoryEntry(0, 'https://www.chromium.org'),
      target: target1,
    };
    listContainer.fire('open-menu', detail1);
    await microtasksFinished();
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
    listContainer.fire('open-menu', detail2);
    await microtasksFinished();
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
