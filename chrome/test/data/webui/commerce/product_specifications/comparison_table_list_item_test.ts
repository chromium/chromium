// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/comparison_table_list_item.js';

import {ProductSpecificationsBrowserProxyImpl} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {ComparisonTableListItemElement} from 'chrome://compare/comparison_table_list_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestProductSpecificationsBrowserProxy} from './test_product_specifications_browser_proxy.js';

class ListItemTestPluralStringProxy extends TestPluralStringProxy {
  override getPluralString(_messageName: string, itemCount: number) {
    return Promise.resolve(`${itemCount} items`);
  }
}

suite('ComparisonTableListItemTest', () => {
  let itemElement: ComparisonTableListItemElement;

  const TABLE_NAME = 'abc';
  const TABLE_UUID = {value: '123'};
  const TABLE_NUM_URLS = 3;
  const TABLE_IMAGE_URL = {url: 'http://example.com/image.png'};
  const TABLE_URL = {url: `chrome://compare/?id=${TABLE_UUID.value}`};

  setup(async () => {
    loadTimeData.overrideValues({
      'tableListItemTitle': `Compare ${TABLE_NAME}`,
    });

    const pluralStringProxy = new ListItemTestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);

    const productSpecsProxy = new TestProductSpecificationsBrowserProxy();
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
    const clickPromise =
        eventToPromise('comparison-table-list-item-click', document);
    itemElement.$.item.click();

    const event = await clickPromise;
    assertEquals(TABLE_UUID, event.detail.uuid);
  });
});
