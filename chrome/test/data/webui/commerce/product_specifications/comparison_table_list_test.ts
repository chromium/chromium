// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/comparison_table_list.js';

import {ProductSpecificationsBrowserProxyImpl} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {ComparisonTableListElement} from 'chrome://compare/comparison_table_list.js';
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
    {
      name: 'pqr',
      uuid: {value: '789'},
      urls: [{url: 'https://example4.com'}],
    },
  ];

  function getListItems() {
    assertTrue(!!listElement);
    return listElement.shadowRoot.querySelectorAll(
        'comparison-table-list-item');
  }

  async function toggleCheckboxAtIndex(index: number) {
    const items = getListItems();
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
    shoppingServiceApi.setResultMapperFor(
        'getProductSpecificationsSetByUuid', (uuid: Uuid) => {
          return {
            set: TABLES.find(table => table.uuid.value === uuid.value),
          };
        });
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

  test('an item is displayed for each table', () => {
    const items = getListItems();

    assertEquals(TABLES.length, items.length);
    for (let i = 0; i < TABLES.length; i++) {
      assertEquals(TABLES[i]!.name, items[i]!.name);
      assertEquals(TABLES[i]!.uuid, items[i]!.uuid);
      assertArrayEquals(TABLES[i]!.urls, items[i]!.urls);
    }
  });

  test(
      'table is deleted when the item\'s delete context menu option is clicked',
      async () => {
        listElement.resetDeletionToastDurationMsForTesting();

        const deleteFinishedPromise =
            eventToPromise('delete-finished-for-testing', listElement);
        const listItem = getListItems()[0];
        assertTrue(!!listItem);
        listItem.fire('delete-table', {
          uuid: TABLES[0]!.uuid,
        });
        await deleteFinishedPromise;

        assertEquals(
            1,
            shoppingServiceApi.getCallCount('deleteProductSpecificationsSet'));
        assertEquals(
            '123',
            shoppingServiceApi.getArgs('deleteProductSpecificationsSet')[0]
                .value);
      });

  suite('multi-select', () => {
    setup(async () => {
      listElement.$.edit.click();
      await microtasksFinished();

      assertEquals(TABLES.length, getListItems().length);
      await toggleCheckboxAtIndex(0);
      await toggleCheckboxAtIndex(1);
    });

    test('displays the number of selected items', () => {
      assertStringContains(listElement.$.toolbar.selectionLabel, '2');
    });

    test('can delete multiple comparison tables', async () => {
      listElement.resetDeletionToastDurationMsForTesting();

      const deleteFinishedPromise =
          eventToPromise('delete-finished-for-testing', listElement);
      listElement.$.delete.click();
      await deleteFinishedPromise;

      assertEquals(
          2, shoppingServiceApi.getCallCount('deleteProductSpecificationsSet'));
      assertArrayEquals(
          [TABLES[0]!.uuid, TABLES[1]!.uuid],
          shoppingServiceApi.getArgs('deleteProductSpecificationsSet'));
    });

    test(
        'deleting a single table when in multi-select hides all checkboxes',
        async () => {
          let items = getListItems();
          assertEquals(TABLES.length, items.length);
          const menu = items[0]!.$.menu.get();
          const deleteButton = menu.querySelector<HTMLButtonElement>('#delete');
          assertTrue(!!deleteButton);
          deleteButton.click();
          await microtasksFinished();

          items = getListItems();
          assertEquals(TABLES.length - 1, items.length);
          for (let i = 0; i < items.length; i++) {
            assertFalse(items[i]!.hasCheckbox);
          }
        });

    suite('deletion toast', () => {
      let toast: CrToastElement;

      setup(async () => {
        // Wait for the toast to be shown after deletion.
        listElement.$.delete.click();
        await microtasksFinished();

        const maybeToast = listElement.$.toast.getIfExists();
        assertTrue(!!maybeToast);
        toast = maybeToast;
        assertTrue(toast.open);
      });

      test('tables pending deletion are hidden', () => {
        // Only one table left after deletion.
        assertEquals(1, getListItems().length);
      });

      test('can undo deletion from toast', async () => {
        const undoButton = toast.querySelector<HTMLButtonElement>('#undo');
        assertTrue(!!undoButton);
        undoButton.click();
        await microtasksFinished();

        // All tables should be restored after deletion.
        assertEquals(3, getListItems().length);
        assertEquals(
            0,
            shoppingServiceApi.getCallCount('deleteProductSpecificationsSet'));
      });

      test(
          'deletion is triggered when navigating away if toast has not ' +
              'disappeared',
          async () => {
            const deleteFinishedPromise =
                eventToPromise('delete-finished-for-testing', listElement);
            window.dispatchEvent(new CustomEvent('beforeunload'));
            await deleteFinishedPromise;

            // Selected sets should be deleted.
            assertEquals(
                2,
                shoppingServiceApi.getCallCount(
                    'deleteProductSpecificationsSet'));
            assertArrayEquals(
                [TABLES[0]!.uuid, TABLES[1]!.uuid],
                shoppingServiceApi.getArgs('deleteProductSpecificationsSet'));
          });
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
        listElement.resetDeletionToastDurationMsForTesting();

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
        assertArrayEquals(
            [TABLES[0]!.uuid, TABLES[1]!.uuid],
            shoppingServiceApi.getArgs('deleteProductSpecificationsSet'));
      });
    });
  });
});
