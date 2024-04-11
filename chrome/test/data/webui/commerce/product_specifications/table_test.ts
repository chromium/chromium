// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/product_specifications/table.js';

import type {TableElement} from 'chrome://compare/product_specifications/table.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('ProductSpecificationsTableTest', () => {
  let tableElement: TableElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    tableElement = document.createElement('product-specifications-table');
    document.body.appendChild(tableElement);
  });

  test('product columns show the correct data', async () => {
    // Arrange.
    const title1 = 'foo';
    const title2 = 'bar';

    // Act.
    tableElement.columns = [{title: title1}, {title: title2}];
    await waitAfterNextRender(tableElement);

    // Assert.
    const columnTitles =
        tableElement.shadowRoot!.querySelectorAll('.col .col-card');
    assertEquals(2, columnTitles.length);
    assertEquals(title1, columnTitles[0]!.textContent);
    assertEquals(title2, columnTitles[1]!.textContent);
  });

  test('product rows show the correct data', async () => {
    // Arrange.
    const row1 = {title: 'foo', values: ['foo1', 'foo2']};
    const row2 = {title: 'bar', values: ['bar2']};

    // Act.
    tableElement.rows = [row1, row2];
    await waitAfterNextRender(tableElement);

    // Assert.
    const rowHeaders =
        tableElement.shadowRoot!.querySelectorAll('.row .row-header');
    assertEquals(2, rowHeaders.length);
    assertEquals(row1.title, rowHeaders[0]!.textContent);
    assertEquals(row2.title, rowHeaders[1]!.textContent);
    const rowContents =
        tableElement.shadowRoot!.querySelectorAll('.row .row-content');
    assertEquals(3, rowContents.length);
    assertEquals(row1.values[0], rowContents[0]!.textContent);
    assertEquals(row1.values[1], rowContents[1]!.textContent);
    assertEquals(row2.values[0], rowContents[2]!.textContent);
  });
});
