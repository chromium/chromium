// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {ProductSpecificationsItemElement} from 'chrome://history/history.js';
import {ShoppingBrowserProxyImpl} from 'chrome://history/history.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {shiftPointerClick} from './test_util.js';


suite('ProductSpecificationsItemTest', () => {
  const shoppingServiceApi = TestMock.fromClass(ShoppingBrowserProxyImpl);
  let productSpecificationsItem: ProductSpecificationsItemElement;

  function createProductSpecsItem() {
    productSpecificationsItem =
        document.createElement('product-specifications-item');
    productSpecificationsItem.item = {
      name: 'example1',
      uuid: {value: 'ex1'},
      urls: [{url: 'dot com 1'}],
    };
    productSpecificationsItem.index = 0;
    document.body.appendChild(productSpecificationsItem);
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    createProductSpecsItem();
    return flushTasks();
  });

  test('render labels', async () => {
    const label = productSpecificationsItem.$.link.textContent!;
    assertEquals('Compare example1 · 1 items', label.trim());
    const url = productSpecificationsItem.$.url.textContent!;
    assertEquals('chrome://compare/?id=ex1', url.trim());
  });

  test('checkbox fires event', async () => {
    let selectionCount = 0;
    let checked = false;
    let uuid = '';
    let shiftKey = false;
    let index = -1;
    productSpecificationsItem.addEventListener(
        'product-spec-item-select', function(e) {
          selectionCount++;
          checked = e.detail.checked;
          uuid = e.detail.uuid;
          shiftKey = e.detail.shiftKey;
          index = e.detail.index;
        });
    const checkbox = productSpecificationsItem.$.checkbox;
    checkbox.click();
    await checkbox.updateComplete;

    assertEquals(1, selectionCount);
    assertEquals(true, checked);
    assertEquals('ex1', uuid);
    assertEquals(false, shiftKey);
    assertEquals(0, index);
  });

  test('menu click fires event', async () => {
    let uuid = '';
    let clicked = false;
    let target = null;
    productSpecificationsItem.addEventListener('item-menu-open', function(e) {
      clicked = true;
      uuid = e.detail.uuid.value;
      target = e.detail.target;
    });
    const menu = productSpecificationsItem.$.menu;
    menu.click();

    assertEquals(true, clicked);
    assertEquals(menu, target);
    assertEquals('ex1', uuid);
  });

  test('focus elements', async () => {
    const focusRow = productSpecificationsItem.createFocusRow();
    const elements = focusRow.getElements();
    assertEquals(3, elements.length);
    assertEquals('checkbox', elements[0]!.id);
    assertEquals('link', elements[1]!.id);
    assertEquals('menu', elements[2]!.id);
  });

  suite('Tests using ShoppingServiceApi', () => {
    suiteSetup(() => {
      shoppingServiceApi.reset();
      ShoppingBrowserProxyImpl.setInstance(shoppingServiceApi);
    });

    test('link click shows product specs table', async () => {
      productSpecificationsItem.$.link.click();

      assertEquals(
          1,
          shoppingServiceApi.getCallCount(
              'showProductSpecificationsSetForUuid'));
      assertDeepEquals(
          [{value: 'ex1'}, true],
          shoppingServiceApi.getArgs('showProductSpecificationsSetForUuid')[0]);
    });

    test('link enter key shows product specs table', async () => {
      shoppingServiceApi.reset();
      pressAndReleaseKeyOn(productSpecificationsItem.$.link, 13, [], 'Enter');
      assertEquals(
          1,
          shoppingServiceApi.getCallCount(
              'showProductSpecificationsSetForUuid'));
      assertDeepEquals(
          [{value: 'ex1'}, true],
          shoppingServiceApi.getArgs('showProductSpecificationsSetForUuid')[0]);
    });
  });

  test('shift on checkbox click', async () => {
    const selectEventPromise =
        eventToPromise('product-spec-item-select', productSpecificationsItem);
    await shiftPointerClick(productSpecificationsItem.$.checkbox);
    const selectEvent = await selectEventPromise;

    assertEquals(true, selectEvent.detail.checked);
    assertEquals('ex1', selectEvent.detail.uuid);
    assertEquals(true, selectEvent.detail.shiftKey);
    assertEquals(0, selectEvent.detail.index);
  });
});
