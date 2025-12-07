// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/comparison_table_list_item.js';

import {ProductSpecificationsBrowserProxyImpl} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import type {ComparisonTableListItemElement} from 'chrome://compare/comparison_table_list_item.js';
import {ShowSetDisposition} from 'chrome://compare/product_specifications.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertArrayEquals, assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestProductSpecificationsBrowserProxy} from './test_product_specifications_browser_proxy.js';

class ListItemTestPluralStringProxy extends TestPluralStringProxy {
  override getPluralString(_messageName: string, itemCount: number) {
    return Promise.resolve(`${itemCount} items`);
  }
}

suite('ComparisonTableListItemTest', () => {
  let itemElement: ComparisonTableListItemElement;
  const productSpecsProxy: TestProductSpecificationsBrowserProxy =
      new TestProductSpecificationsBrowserProxy();
  const shoppingServiceApi =
      TestMock.fromClass(ShoppingServiceBrowserProxyImpl);

  const TABLE_NAME = 'abc';
  const TABLE_UUID = {value: '123'};
  const TABLE_URLS = [
    {url: 'https://example1.com'},
    {url: 'https://example2.com'},
    {url: 'https://example3.com'},
  ];
  const TABLE_URL = {url: `chrome://compare/?id=${TABLE_UUID.value}`};

  function getTrailingIconButton() {
    const trailingIconButton =
        $$<CrIconButtonElement>(itemElement, '#trailingIconButton');
    assertTrue(!!trailingIconButton);
    return trailingIconButton;
  }

  function getCheckbox(): CrCheckboxElement {
    const checkbox = $$<CrCheckboxElement>(itemElement, '#checkbox');
    assertTrue(!!checkbox);
    return checkbox;
  }

  async function createItemElement() {
    const imageUpdatedPromise =
        eventToPromise('image-updated-for-testing', document);
    const numItemsUpdatedPromise =
        eventToPromise('num-items-updated-for-testing', document);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    itemElement = document.createElement('comparison-table-list-item');
    itemElement.name = TABLE_NAME;
    itemElement.uuid = TABLE_UUID;
    itemElement.urls = TABLE_URLS;
    document.body.appendChild(itemElement);

    await Promise.all([imageUpdatedPromise, numItemsUpdatedPromise]);
  }

  setup(() => {
    loadTimeData.overrideValues({
      'tableListItemTitle': `Compare ${TABLE_NAME}`,
    });

    const pluralStringProxy = new ListItemTestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);

    productSpecsProxy.reset();
    ProductSpecificationsBrowserProxyImpl.setInstance(productSpecsProxy);

    shoppingServiceApi.reset();
    shoppingServiceApi.setResultMapperFor(
        'getProductInfoForUrl', (url: Url) => {
          return Promise.resolve({
            productInfo: {
              imageUrl: {url: `${url.url}/image.png`},
            },
          });
        });
    ShoppingServiceBrowserProxyImpl.setInstance(shoppingServiceApi);
  });

  test('name, link, number of URLS, and image are displayed', async () => {
    await createItemElement();
    const urlListItem = itemElement.$.item;

    assertStringContains(urlListItem.title, TABLE_NAME);
    assertEquals(TABLE_URL.url, urlListItem.description);
    assertTrue(!!itemElement.$.numItems.textContent);
    assertStringContains(
        itemElement.$.numItems.textContent, `${TABLE_URLS.length}`);
    assertTrue(
        urlListItem.imageUrls.includes(`${TABLE_URLS[0]!.url}/image.png`));
  });

  test('first available product image is used', async () => {
    shoppingServiceApi.setResultMapperFor(
        'getProductInfoForUrl', (url: Url) => {
          // First product has no image.
          if (url.url === TABLE_URLS[0]!.url) {
            return {
              productInfo: {
                imageUrl: '',
              },
            };
          }

          // Other products have an image.
          return Promise.resolve({
            productInfo: {
              imageUrl: {url: `${url.url}/image.png`},
            },
          });
        });
    await createItemElement();
    const urlListItem = itemElement.$.item;

    // Second product image should be used.
    assertTrue(
        urlListItem.imageUrls.includes(`${TABLE_URLS[1]!.url}/image.png`));
  });

  test('favicon is used if no product image is available', async () => {
    shoppingServiceApi.setResultMapperFor('getProductInfoForUrl', () => {
      // No products have an image.
      return Promise.resolve({
        productInfo: {
          imageUrl: '',
        },
      });
    });
    await createItemElement();
    const urlListItem = itemElement.$.item;

    // If image URL is empty but URL is not, the favicon of the URL is
    // displayed.
    assertEquals(0, urlListItem.imageUrls.length);
    assertEquals(TABLE_URLS[0]!.url, urlListItem.url);
  });

  test('click emits event with UUID', async () => {
    const clickPromise = eventToPromise('item-click', document);
    itemElement.$.item.click();

    const event = await clickPromise;
    assertEquals(TABLE_UUID, event.detail.uuid);
  });

  suite('context menu', () => {
    let menu: CrActionMenuElement;

    setup(() => {
      getTrailingIconButton().click();

      const maybeMenu = itemElement.$.menu.getIfExists();
      assertTrue(!!maybeMenu);
      menu = maybeMenu;
    });

    test('open in new tab option opens the table in a new tab', () => {
      const openInNewTabButton =
          menu.querySelector<HTMLButtonElement>('#openInNewTab');
      assertTrue(!!openInNewTabButton);
      openInNewTabButton.click();

      assertEquals(
          1,
          productSpecsProxy.getCallCount(
              'showProductSpecificationsSetForUuid'));
      const args =
          productSpecsProxy.getArgs('showProductSpecificationsSetForUuid')[0];
      assertEquals(TABLE_UUID, args[0]);
      assertEquals(/*inNewTab=*/ true, args[1]);
    });

    test(
        'open in new window option opens the table in a new window', () => {
          const openInNewWindowButton =
              menu.querySelector<HTMLButtonElement>('#openInNewWindow');
          assertTrue(!!openInNewWindowButton);
          openInNewWindowButton.click();

          assertEquals(
              1,
              productSpecsProxy.getCallCount(
                  'showProductSpecificationsSetsForUuids'));
          const args = productSpecsProxy.getArgs(
              'showProductSpecificationsSetsForUuids')[0];
          assertArrayEquals([TABLE_UUID], args[0]);
          assertEquals(ShowSetDisposition.kInNewWindow, args[1]);
        });

    test(
        'rename option displays input, and submitting emits event with UUID ' +
            'and name',
        async () => {
          const newTableName = 'xyz';
          const renamePromise = eventToPromise('rename-table', document);

          const renameButton = menu.querySelector<HTMLButtonElement>('#rename');
          assertTrue(!!renameButton);
          renameButton.click();
          await microtasksFinished();

          const input = $$<CrInputElement>(itemElement, '#renameInput');
          assertTrue(!!input);
          input.value = newTableName;
          input.focus();
          input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

          const event = await renamePromise;
          assertEquals(TABLE_UUID, event.detail.uuid);
          assertEquals(newTableName, event.detail.name);
        });

    test('delete option emits event with UUID', async () => {
      const deletePromise = eventToPromise('delete-table', document);

      const deleteButton = menu.querySelector<HTMLButtonElement>('#delete');
      assertTrue(!!deleteButton);
      deleteButton.click();

      const event = await deletePromise;
      assertEquals(TABLE_UUID, event.detail.uuid);
    });
  });

  suite('checkbox', () => {
    let checkbox: CrCheckboxElement;

    setup(async () => {
      itemElement.hasCheckbox = true;
      await microtasksFinished();

      checkbox = getCheckbox();
    });

    test(
        'click on checkbox emits event with UUID and checked state',
        async () => {
          let checkboxChangePromise =
              eventToPromise('checkbox-change', document);
          checkbox.click();

          let event = await checkboxChangePromise;
          assertEquals(TABLE_UUID, event.detail.uuid);
          assertTrue(event.detail.checked);

          // Uncheck.
          checkboxChangePromise = eventToPromise('checkbox-change', document);
          checkbox.click();

          event = await checkboxChangePromise;
          assertEquals(TABLE_UUID, event.detail.uuid);
          assertFalse(event.detail.checked);
        });

    test('click on item emits event with UUID and checked state', async () => {
      let checkboxChangePromise = eventToPromise('checkbox-change', document);
      checkbox.click();

      let event = await checkboxChangePromise;
      assertEquals(TABLE_UUID, event.detail.uuid);
      assertTrue(event.detail.checked);

      // Uncheck.
      checkboxChangePromise = eventToPromise('checkbox-change', document);
      checkbox.click();

      event = await checkboxChangePromise;
      assertEquals(TABLE_UUID, event.detail.uuid);
      assertFalse(event.detail.checked);
    });

    test(
        'rename menu item is disabled when the checkbox is visible',
        async () => {
          itemElement.$.item.dispatchEvent(new MouseEvent('contextmenu'));
          await microtasksFinished();

          const menu = itemElement.$.menu.getIfExists();
          assertTrue(!!menu);
          const renameButton = menu.querySelector<HTMLButtonElement>('#rename');
          assertTrue(!!renameButton);
          assertTrue(renameButton.disabled);
        });
  });
});
