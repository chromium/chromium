// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {ProductSpecificationsItemElement} from 'chrome://history/history.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('ProductSpecificationsItemTest', () => {
  let productSpecificationsItem: ProductSpecificationsItemElement;

  function createProductSpecsItem() {
    productSpecificationsItem =
        document.createElement('product-specifications-item');
    productSpecificationsItem.productSet = {
      name: 'example1',
      uuid: {value: 'ex1'},
      urls: [{url: 'dot com 1'}],
    };
    document.body.appendChild(productSpecificationsItem);
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    createProductSpecsItem();
    return flushTasks();
  });

  test('render labels', async () => {
    const label = productSpecificationsItem.$.link.textContent!;
    assertEquals('Analyze example1 · 1 items ex1', label.trim());
  });

  test('checkbox fires event', async () => {
    let selectionCount = 0;
    let checked = false;
    let uuid = '';
    productSpecificationsItem.addEventListener(
        'item-checkbox-select', function(e) {
          selectionCount++;
          checked = e.detail.checked;
          uuid = e.detail.uuid;
        });
    const checkbox = productSpecificationsItem.$.checkbox;
    checkbox.click();
    await checkbox.updateComplete;

    assertEquals(1, selectionCount);
    assertEquals(true, checked);
    assertEquals('ex1', uuid);
  });

  test('focus elements', async () => {
    const focusRow = productSpecificationsItem.createFocusRow();
    const elements = focusRow.getElements();
    assertEquals(2, elements.length);
    assertEquals('checkbox', elements[0]!.id);
    assertEquals('link', elements[1]!.id);
  });
});
