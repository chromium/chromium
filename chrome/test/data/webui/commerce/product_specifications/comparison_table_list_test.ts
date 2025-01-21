// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/comparison_table_list.js';

import {ProductSpecificationsBrowserProxyImpl} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {ComparisonTableListElement} from 'chrome://compare/comparison_table_list.js';
import type {ComparisonTableListItemElement} from 'chrome://compare/comparison_table_list_item.js';
import {ShowSetDisposition} from 'chrome://compare/product_specifications.mojom-webui.js';
import {assertArrayEquals, assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestProductSpecificationsBrowserProxy} from './test_product_specifications_browser_proxy.js';

suite('ComparisonTableListTest', () => {
  let listElement: ComparisonTableListElement;
  const shoppingServiceApi =
      TestMock.fromClass(ShoppingServiceBrowserProxyImpl);
  const productSpecsProxy = new TestProductSpecificationsBrowserProxy();

  const TABLES = [
    {
      name: 'abc',
      uuid: {value: '123'},
      urls: [
        {url: 'https://example1.com'},
        {url: 'https://example2.com'},
        {url: 'https://example3.com'},
      ],
    },
    {
      name: 'xyz',
      uuid: {value: '456'},
      urls: [
        {url: 'https://example1.com'},
        {url: 'https://example2.com'},
      ],
    },
  ];

  async function toggleCheckboxAtIndex(index: number) {
    const items =
        listElement.shadowRoot!.querySelectorAll('comparison-table-list-item');
    assertTrue(index >= 0 && index < items.length);

    const checkboxChangePromise =
        eventToPromise('checkbox-change', listElement);
    const checkbox = $$(items[index]!, '#checkbox');
    assertTrue(!!checkbox);
    checkbox.click();

    const event = await checkboxChangePromise;
    assertTrue(!!event);
  }

  setup(async () => {
    // Used by the item elements in the list.
    const pluralStringProxy = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);

    productSpecsProxy.reset();
    ProductSpecificationsBrowserProxyImpl.setInstance(productSpecsProxy);

    shoppingServiceApi.reset();
    shoppingServiceApi.setResultFor(
        'deleteProductSpecificationsSet', Promise.resolve());
    shoppingServiceApi.setResultFor('getProductInfoForUrl', Promise.resolve({
      productInfo: {
        imageUrl: {url: 'https://example.com/image.png'},
      },
    }));
    ShoppingServiceBrowserProxyImpl.setInstance(shoppingServiceApi);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    listElement = document.createElement('comparison-table-list');
    listElement.tables = TABLES;
    document.body.appendChild(listElement);

    await microtasksFinished();
  });

  test('an item is displayed for each table', async () => {
    const items =
        listElement.shadowRoot!.querySelectorAll('comparison-table-list-item');

    assertEquals(TABLES.length, items.length);
    for (let i = 0; i < TABLES.length; i++) {
      assertEquals(TABLES[i]!.name, items[i]!.name);
      assertEquals(TABLES[i]!.uuid, items[i]!.uuid);
      assertArrayEquals(TABLES[i]!.urls, items[i]!.urls);
    }
  });

  suite('multi-select', () => {
    let items: NodeListOf<ComparisonTableListItemElement>;

    setup(async () => {
      listElement.$.edit.click();
      await microtasksFinished();

      items = listElement.shadowRoot!.querySelectorAll(
          'comparison-table-list-item');
      assertEquals(TABLES.length, items.length);
      await toggleCheckboxAtIndex(0);
      await toggleCheckboxAtIndex(1);
    });

    test('displays the number of selected items', async () => {
      assertStringContains(listElement.$.toolbar.selectionLabel, '2');
    });

    test('can delete multiple comparison tables', async () => {
      const deleteFinishedPromise =
          eventToPromise('delete-finished-for-testing', listElement);
      listElement.$.delete.click();
      await deleteFinishedPromise;

      assertEquals(
          2, shoppingServiceApi.getCallCount('deleteProductSpecificationsSet'));
      assertEquals(
          TABLES[0]!.uuid,
          shoppingServiceApi.getArgs('deleteProductSpecificationsSet')[0]);
      assertEquals(
          TABLES[1]!.uuid,
          shoppingServiceApi.getArgs('deleteProductSpecificationsSet')[1]);
    });

    test(
        'deleting a single table when in multi-select hides all checkboxes',
        async () => {
          const menu = items[0]!.$.menu.get();
          const deleteButton = menu.querySelector<HTMLButtonElement>('#delete');
          assertTrue(!!deleteButton);
          deleteButton.click();
          await microtasksFinished();

          for (let i = 0; i < items.length; i++) {
            assertFalse(items[i]!.hasCheckbox);
          }
        });


    suite('context menu', () => {
      let menu: CrActionMenuElement;

      setup(async () => {
        listElement.$.more.click();
        await microtasksFinished();

        menu = listElement.$.menu.get();
        assertTrue(!!menu);
      });

      test('can open multiple comparison tables', async () => {
        const openAllFinishedPromise =
            eventToPromise('open-all-finished-for-testing', listElement);
        const openAllButton =
            menu.querySelector<HTMLButtonElement>('#menuOpenAll');
        assertTrue(!!openAllButton);
        openAllButton.click();
        await openAllFinishedPromise;

        assertEquals(
            1,
            productSpecsProxy.getCallCount(
                'showProductSpecificationsSetsForUuids'));
        const args = productSpecsProxy.getArgs(
            'showProductSpecificationsSetsForUuids')[0];
        assertArrayEquals([TABLES[0]!.uuid, TABLES[1]!.uuid], args[0]);
        assertEquals(ShowSetDisposition.kInNewTabs, args[1]);
      });

      test('can open multiple comparison tables in a new window', async () => {
        const openAllInNewWindowFinishedPromise = eventToPromise(
            'open-all-in-new-window-finished-for-testing', listElement);
        const openAllInNewWindowButton =
            menu.querySelector<HTMLButtonElement>('#menuOpenAllInNewWindow');
        assertTrue(!!openAllInNewWindowButton);
        openAllInNewWindowButton.click();
        await openAllInNewWindowFinishedPromise;

        assertEquals(
            1,
            productSpecsProxy.getCallCount(
                'showProductSpecificationsSetsForUuids'));
        const args = productSpecsProxy.getArgs(
            'showProductSpecificationsSetsForUuids')[0];
        assertArrayEquals([TABLES[0]!.uuid, TABLES[1]!.uuid], args[0]);
        assertEquals(ShowSetDisposition.kInNewWindow, args[1]);
      });

      test('can delete multiple comparison tables', async () => {
        const deleteFinishedPromise =
            eventToPromise('delete-finished-for-testing', listElement);
        const deleteButton =
            menu.querySelector<HTMLButtonElement>('#menuDelete');
        assertTrue(!!deleteButton);
        deleteButton.click();
        await deleteFinishedPromise;

        assertEquals(
            2,
            shoppingServiceApi.getCallCount('deleteProductSpecificationsSet'));
        assertEquals(
            TABLES[0]!.uuid,
            shoppingServiceApi.getArgs('deleteProductSpecificationsSet')[0]);
        assertEquals(
            TABLES[1]!.uuid,
            shoppingServiceApi.getArgs('deleteProductSpecificationsSet')[1]);
      });
    });
  });
});
