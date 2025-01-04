// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/comparison_table_list.js';

import {ProductSpecificationsBrowserProxyImpl} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {ComparisonTableListElement} from 'chrome://compare/comparison_table_list.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestProductSpecificationsBrowserProxy} from './test_product_specifications_browser_proxy.js';

suite('ComparisonTableListTest', () => {
  let listElement: ComparisonTableListElement;

  const TABLES = [
    {
      name: 'abc',
      uuid: {value: '123'},
      numUrls: 3,
      imageUrl: {url: 'http://example.com/image1.png'},
    },
    {
      name: 'xyz',
      uuid: {value: '456'},
      numUrls: 2,
      imageUrl: {url: 'http://example.com/image2.png'},
    },
  ];

  setup(async () => {
    // Used by the item elements in the list.
    const pluralStringProxy = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);

    const productSpecsProxy = new TestProductSpecificationsBrowserProxy();
    ProductSpecificationsBrowserProxyImpl.setInstance(productSpecsProxy);

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
      assertEquals(TABLES[i]!.numUrls, items[i]!.numUrls);
      assertEquals(TABLES[i]!.imageUrl, items[i]!.imageUrl);
    }
  });
});
