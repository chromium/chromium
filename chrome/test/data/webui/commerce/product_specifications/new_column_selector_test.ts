// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/new_column_selector.js';

import type {NewColumnSelectorElement} from 'chrome://compare/new_column_selector.js';
import {ShoppingServiceBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewColumnSelectorTest', () => {
  const shoppingServiceApi =
      TestMock.fromClass(ShoppingServiceBrowserProxyImpl);
  const SHOWING_MENU_CLASS = 'showing-menu';

  async function createSelector(): Promise<NewColumnSelectorElement> {
    const selector = document.createElement('new-column-selector');
    document.body.appendChild(selector);
    await microtasksFinished();
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

  setup(() => {
    shoppingServiceApi.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    ShoppingServiceBrowserProxyImpl.setInstance(shoppingServiceApi);
  });

  test('menu shown on enter', async () => {
    initUrlInfos();
    const selector = await createSelector();
    const menu = selector.$.productSelectionMenu;

    assertEquals(menu.$.menu.getIfExists(), null);
    assertFalse(selector.$.button.classList.contains(SHOWING_MENU_CLASS));

    selector.$.button.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    assertNotEquals(menu.$.menu.getIfExists(), null);
    assertTrue(selector.$.button.classList.contains(SHOWING_MENU_CLASS));
  });

  test('updates showing menu class', async () => {
    initUrlInfos();
    const selector = await createSelector();

    assertFalse(selector.$.button.classList.contains(SHOWING_MENU_CLASS));

    selector.$.button.click();
    await microtasksFinished();

    assertTrue(selector.$.button.classList.contains(SHOWING_MENU_CLASS));

    selector.$.productSelectionMenu.close();
    await eventToPromise('close', selector.$.productSelectionMenu);

    assertFalse(selector.$.button.classList.contains(SHOWING_MENU_CLASS));
  });
});
