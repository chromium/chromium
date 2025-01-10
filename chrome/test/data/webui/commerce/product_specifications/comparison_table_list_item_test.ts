// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/comparison_table_list_item.js';

import {ProductSpecificationsBrowserProxyImpl} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {ComparisonTableListItemElement} from 'chrome://compare/comparison_table_list_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  const TABLE_NAME = 'abc';
  const TABLE_UUID = {value: '123'};
  const TABLE_NUM_URLS = 3;
  const TABLE_IMAGE_URL = {url: 'http://example.com/image.png'};
  const TABLE_URL = {url: `chrome://compare/?id=${TABLE_UUID.value}`};

  function getTrailingIconButton() {
    const trailingIconButton =
        $$<CrIconButtonElement>(itemElement, '#trailingIconButton');
    assertTrue(!!trailingIconButton);
    return trailingIconButton;
  }

  setup(async () => {
    loadTimeData.overrideValues({
      'tableListItemTitle': `Compare ${TABLE_NAME}`,
    });

    const pluralStringProxy = new ListItemTestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);

    productSpecsProxy.reset();
    ProductSpecificationsBrowserProxyImpl.setInstance(productSpecsProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    itemElement = document.createElement('comparison-table-list-item');
    itemElement.name = TABLE_NAME;
    itemElement.uuid = TABLE_UUID;
    itemElement.numUrls = TABLE_NUM_URLS;
    itemElement.imageUrl = TABLE_IMAGE_URL;
    document.body.appendChild(itemElement);

    await microtasksFinished();
  });

  test('name, link, number of URLS, and image are displayed', async () => {
    const urlListItem = itemElement.$.item;

    assertStringContains(urlListItem.title, TABLE_NAME);
    assertEquals(TABLE_URL.url, urlListItem.url);
    assertTrue(!!itemElement.$.numItems.textContent);
    assertStringContains(
        itemElement.$.numItems.textContent, `${TABLE_NUM_URLS}`);
    assertTrue(urlListItem.imageUrls.includes(TABLE_IMAGE_URL.url));
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

    test('open in new tab option opens the table in a new tab', async () => {
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
        'rename option displays input, and submitting emits event with UUID ' +
            'and name',
        async () => {
          const newTableName = 'xyz';
          const renamePromise = eventToPromise('rename-table', document);

          const renameButton = menu.querySelector<HTMLButtonElement>('#rename');
          assertTrue(!!renameButton);
          renameButton!.click();
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
      deleteButton!.click();

      const event = await deletePromise;
      assertEquals(TABLE_UUID, event.detail.uuid);
    });
  });
});
