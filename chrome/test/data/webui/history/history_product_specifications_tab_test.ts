// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import {ShoppingBrowserProxyImpl} from 'chrome://history/history.js';
import type {ProductSpecificationsListsElement} from 'chrome://history/history.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

suite('ProductSpecificationsListTest', () => {
  const shoppingServiceApi = TestMock.fromClass(ShoppingBrowserProxyImpl);
  let productSpecificationsList: ProductSpecificationsListsElement;

  function createProductSpecsList(): ProductSpecificationsListsElement {
    productSpecificationsList =
        document.createElement('product-specifications-lists');
    document.body.appendChild(productSpecificationsList);
    return productSpecificationsList;
  }

  function initProductSets() {
    shoppingServiceApi.setResultFor(
        'getAllProductSpecificationsSets', Promise.resolve({
          sets: [
            {
              name: 'example1',
              uuid: {value: 'ex1'},
              urls: ['dot com 1'],
            },
            {
              name: 'example2',
              uuid: {value: 'ex2'},
              urls: ['dot com 2'],
            },
          ],
        }));
  }

  setup(function() {
    shoppingServiceApi.reset();
    ShoppingBrowserProxyImpl.setInstance(shoppingServiceApi);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('load', async () => {
    initProductSets();
    createProductSpecsList();
    await flushTasks();
    await shoppingServiceApi.whenCalled('getAllProductSpecificationsSets');
    const items = productSpecificationsList.shadowRoot!.querySelectorAll(
        'product-specifications-item');
    assertEquals(2, items.length);
    const firstSet = items[0]!.productSet;
    assertEquals('example1', firstSet.name);
    assertEquals('ex1', firstSet.uuid.value);
    assertEquals('dot com 1', firstSet.urls[0]);

    const secondSet = items[1]!.productSet;
    assertEquals('example2', secondSet.name);
    assertEquals('ex2', secondSet.uuid.value);
    assertEquals('dot com 2', secondSet.urls[0]);
  });
});
