// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/product_selector.js';

import type {ProductSelectorElement} from 'chrome://compare/product_selector.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {stringToMojoString16, stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('ProductSelectorTest', () => {
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);

  async function createSelector(): Promise<ProductSelectorElement> {
    const selector = document.createElement('product-selector');
    document.body.appendChild(selector);
    await flushTasks();
    return selector;
  }

  setup(async () => {
    shoppingServiceApi.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(shoppingServiceApi);
  });

  test('OpenTabsShown', async () => {
    const titleString = 'title';
    const openTabs = [{
      title: stringToMojoString16(titleString),
      url: stringToMojoUrl('http://example.com'),
    }];
    const selector = await createSelector();

    shoppingServiceApi.setResultFor(
        'getUrlInfosForOpenTabs', Promise.resolve({urlInfos: openTabs}));

    assertEquals(0, shoppingServiceApi.getCallCount('getUrlInfosForOpenTabs'));

    selector.$.currentItemButton.click();

    await shoppingServiceApi.whenCalled('getUrlInfosForOpenTabs');

    await flushTasks();

    // Ensure the number of list items is equal to the number of open tabs.
    assertEquals(openTabs.length, selector.openTabs.length);

    assertEquals(titleString, selector.openTabs[0]!.title);
    assertEquals(openTabs[0]!.url.url, selector.openTabs[0]!.url);
  });
});
