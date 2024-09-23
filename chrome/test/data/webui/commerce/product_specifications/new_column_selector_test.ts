// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/new_column_selector.js';

import type {NewColumnSelectorElement} from 'chrome://compare/new_column_selector.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('NewColumnSelectorTest', () => {
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);

  async function createSelector(): Promise<NewColumnSelectorElement> {
    const selector = document.createElement('new-column-selector');
    document.body.appendChild(selector);
    await flushTasks();
    return selector;
  }

  function initUrlInfos() {
    const productTabs = [{
      title: 'title',
      url: stringToMojoUrl('http://example.com'),
    }];
    const recentlyViewedTabs = [{
      title: 'title2',
      url: stringToMojoUrl('http://example2.com'),
    }];
    shoppingServiceApi.setResultFor(
        'getUrlInfosForProductTabs', Promise.resolve({urlInfos: productTabs}));
    shoppingServiceApi.setResultFor(
        'getUrlInfosForRecentlyViewedTabs',
        Promise.resolve({urlInfos: recentlyViewedTabs}));
  }

  setup(async () => {
    shoppingServiceApi.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(shoppingServiceApi);
  });

  test('menu shown on enter', async () => {
    initUrlInfos();
    const selector = await createSelector();
    const showingMenuClass = 'showing-menu';
    const menu = selector.$.productSelectionMenu;

    assertEquals(menu.$.menu.getIfExists(), null);
    assertFalse(selector.$.button.classList.contains(showingMenuClass));

    selector.$.button.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await flushTasks();

    assertNotEquals(menu.$.menu.getIfExists(), null);
    assertTrue(selector.$.button.classList.contains(showingMenuClass));
  });

  test('updates showing menu class', async () => {
    initUrlInfos();
    const selector = await createSelector();
    const showingMenuClass = 'showing-menu';

    assertFalse(selector.$.button.classList.contains(showingMenuClass));

    selector.$.button.click();
    await flushTasks();

    assertTrue(selector.$.button.classList.contains(showingMenuClass));

    selector.$.productSelectionMenu.close();
    await eventToPromise('close', selector.$.productSelectionMenu);

    assertFalse(selector.$.button.classList.contains(showingMenuClass));
  });
});
