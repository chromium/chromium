// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/table.js';

import type {TableElement} from 'chrome://compare/table.js';
import type {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('ProductSpecificationsTableTest', () => {
  let tableElement: TableElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    tableElement = document.createElement('product-specifications-table');
    document.body.appendChild(tableElement);
  });

  test('column count correct', async () => {
    // Arrange / Act.
    tableElement.columns = [
      {
        selectedItem:
            {title: 'title', url: 'https://example.com/1', imageUrl: ''},
      },
      {
        selectedItem:
            {title: 'title2', url: 'https://example.com/2', imageUrl: ''},
      },
    ];
    await waitAfterNextRender(tableElement);

    // Assert.
    const columns = tableElement.shadowRoot!.querySelectorAll('.col');
    assertEquals(2, columns.length);
  });

  test('images are displayed', async () => {
    // Arrange / Act.
    tableElement.columns = [
      {
        selectedItem: {
          title: 'title',
          url: 'https://example.com/1',
          imageUrl: 'https://foo.com/image',
        },
      },
      {
        selectedItem: {
          title: 'title2',
          url: 'https://example.com/2',
          imageUrl: 'https://bar.com/image',
        },
      },
    ];
    await waitAfterNextRender(tableElement);

    // Assert.
    const columnImages =
        tableElement.shadowRoot!.querySelectorAll<CrAutoImgElement>('.col img');
    assertEquals(2, columnImages.length);
    assertEquals(
        tableElement.columns[0]!.selectedItem.imageUrl,
        columnImages[0]!.autoSrc);
    assertEquals(
        tableElement.columns[1]!.selectedItem.imageUrl,
        columnImages[1]!.autoSrc);
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
