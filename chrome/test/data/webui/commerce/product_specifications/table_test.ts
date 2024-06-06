// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/table.js';

import type {TableElement} from 'chrome://compare/table.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {$$, assertNotStyle, assertStyle} from './test_support.js';

suite('ProductSpecificationsTableTest', () => {
  let tableElement: TableElement;
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);

  setup(async () => {
    shoppingServiceApi.reset();
    BrowserProxyImpl.setInstance(shoppingServiceApi);
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

    const rows = tableElement.shadowRoot!.querySelectorAll('.row');
    assertEquals(0, rows.length);
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
    const row1 = {
      title: 'foo',
      descriptions: ['foo1', 'foo2'],
      summaries: ['summary1', 'summary2'],
    };
    const row2 = {
      title: 'bar',
      descriptions: ['bar2'],
      summaries: ['summary3'],
    };

    // Act.
    tableElement.rows = [row1, row2];
    await waitAfterNextRender(tableElement);

    // Assert.

    // Since no column headers were specified, that section should remain empty.
    const columnHeads = tableElement.shadowRoot!.querySelectorAll('.col');
    assertEquals(0, columnHeads.length);

    const rowHeaders =
        tableElement.shadowRoot!.querySelectorAll('.row .row-header');
    assertEquals(2, rowHeaders.length);
    assertEquals(row1.title, rowHeaders[0]!.textContent);
    assertEquals(row2.title, rowHeaders[1]!.textContent);
    const rowContents =
        tableElement.shadowRoot!.querySelectorAll('.row .row-content');
    assertEquals(3, rowContents.length);
    assertEquals(row1.descriptions[0], rowContents[0]!.textContent);
    assertEquals(row1.descriptions[1], rowContents[1]!.textContent);
    assertEquals(row2.descriptions[0], rowContents[2]!.textContent);

    const rowSummary =
        tableElement.shadowRoot!.querySelectorAll('.row-summary');
    assertEquals(3, rowSummary.length);
    assertEquals(row1.summaries[0], rowSummary[0]!.textContent);
    assertEquals(row1.summaries[1], rowSummary[1]!.textContent);
    assertEquals(row2.summaries[0], rowSummary[2]!.textContent);
  });

  test('fires url change event', async () => {
    // Arrange
    tableElement.columns = [
      {selectedItem: {title: 'title', url: 'https://foo.com', imageUrl: ''}},
      {selectedItem: {title: 'title2', url: 'https://bar.com', imageUrl: ''}},
    ];
    await waitAfterNextRender(tableElement);

    // Act
    const productSelector =
        tableElement.shadowRoot!.querySelector('product-selector');
    assertTrue(!!productSelector);
    const eventPromise = eventToPromise('url-change', tableElement);
    productSelector.dispatchEvent(new CustomEvent('selected-url-change', {
      detail: {
        url: 'https://foo.com',
      },
    }));

    // Assert.
    const event = await eventPromise;
    assertTrue(!!event);
    assertEquals('https://foo.com', event.detail.url);
    assertEquals(0, event.detail.index);
  });

  test('fires url remove event', async () => {
    // Arrange
    tableElement.columns = [
      {selectedItem: {title: 'title', url: 'https://foo.com', imageUrl: ''}},
      {selectedItem: {title: 'title2', url: 'https://bar.com', imageUrl: ''}},
    ];
    await waitAfterNextRender(tableElement);
    const productSelector =
        tableElement.shadowRoot!.querySelector('product-selector');
    assertTrue(!!productSelector);
    const eventPromise = eventToPromise('url-remove', tableElement);
    productSelector.dispatchEvent(new CustomEvent('remove-url'));

    // Assert.
    const event = await eventPromise;
    assertTrue(!!event);
    assertEquals(0, event.detail.index);
  });

  test('opens tab when `openTabButton` is clicked', async () => {
    // Arrange
    const testUrl = 'https://example.com';
    tableElement.columns = [
      {
        selectedItem: {
          title: 'title',
          url: testUrl,
          imageUrl: 'https://example.com/image',
        },
      },
      {
        selectedItem: {
          title: 'title2',
          url: 'https://example.com/2',
          imageUrl: 'https://example.com/2/image',
        },
      },
    ];
    await waitAfterNextRender(tableElement);

    // Act
    const openTabButton = $$<HTMLElement>(tableElement, '#openTabButton');
    assertTrue(!!openTabButton);
    openTabButton!.click();

    // Assert.
    assertEquals(1, shoppingServiceApi.getCallCount('switchToOrOpenTab'));
    assertEquals(
        testUrl, shoppingServiceApi.getArgs('switchToOrOpenTab')[0].url);
  });

  // Disabled for flakiness, see b/342635241.
  test.skip('shows open tab button when hovered', async () => {
    // Arrange
    tableElement.columns = [
      {
        selectedItem: {
          title: 'title',
          url: 'https://example.com',
          imageUrl: 'https://example.com/image',
        },
      },
      {
        selectedItem: {
          title: 'title2',
          url: 'https://example.com/2',
          imageUrl: 'https://example.com/2/image',
        },
      },
    ];
    tableElement.rows = [
      {
        title: 'foo',
        descriptions: ['foo1', 'foo2'],
        summaries: ['summary1', 'summary2'],
      },
      {title: 'bar', descriptions: ['bar2'], summaries: ['summary3']},
    ];
    await waitAfterNextRender(tableElement);
    const columns = tableElement.shadowRoot!.querySelectorAll('.col');
    assertEquals(2, columns.length);
    const openTabButton1 =
        columns[0]!.querySelector<HTMLElement>('#openTabButton');
    assertTrue(!!openTabButton1);
    const openTabButton2 =
        columns[1]!.querySelector<HTMLElement>('#openTabButton');
    assertTrue(!!openTabButton2);
    assertStyle(openTabButton1, 'display', 'none');
    assertStyle(openTabButton2, 'display', 'none');

    // Act/Assert
    columns[0]!.dispatchEvent(new PointerEvent('pointerenter'));
    assertNotStyle(openTabButton1, 'display', 'none');
    assertStyle(openTabButton2, 'display', 'none');

    columns[1]!.dispatchEvent(new PointerEvent('pointerenter'));
    assertStyle(openTabButton1, 'display', 'none');
    assertNotStyle(openTabButton2, 'display', 'none');

    const rowContent =
        tableElement.shadowRoot!.querySelectorAll('.row-content');
    assertEquals(3, rowContent.length);
    // |rowContent[0]| shows underneath the first column.
    rowContent[0]!.dispatchEvent(new PointerEvent('pointerenter'));
    assertNotStyle(openTabButton1, 'display', 'none');
    assertStyle(openTabButton2, 'display', 'none');

    // |rowContent[1]| shows underneath the second column.
    rowContent[1]!.dispatchEvent(new PointerEvent('pointerenter'));
    assertStyle(openTabButton1, 'display', 'none');
    assertNotStyle(openTabButton2, 'display', 'none');

    // |rowContent[2]| shows underneath the first column.
    rowContent[2]!.dispatchEvent(new PointerEvent('pointerenter'));
    assertNotStyle(openTabButton1, 'display', 'none');
    assertStyle(openTabButton2, 'display', 'none');

    const table = tableElement.shadowRoot!.querySelector('table');
    assertTrue(!!table);
    table!.dispatchEvent(new PointerEvent('pointerleave'));
    assertStyle(openTabButton1, 'display', 'none');
    assertStyle(openTabButton2, 'display', 'none');

    // Check that summaries also show the new tab button.
    const rowSummary =
        tableElement.shadowRoot!.querySelectorAll('.row-summary');
    assertEquals(3, rowSummary.length);
    rowSummary[0]!.dispatchEvent(new PointerEvent('pointerenter'));
    assertNotStyle(openTabButton1, 'display', 'none');
    assertStyle(openTabButton2, 'display', 'none');

    rowSummary[1]!.dispatchEvent(new PointerEvent('pointerenter'));
    assertStyle(openTabButton1, 'display', 'none');
    assertNotStyle(openTabButton2, 'display', 'none');

    const summaryElements =
        tableElement.shadowRoot!.querySelectorAll('.row-summary');
    assertEquals(3, summaryElements.length);
  });

  test('Summaries excluded if empty', async () => {
    // Arrange
    tableElement.columns = [
      {
        selectedItem: {
          title: 'title',
          url: 'https://example.com',
          imageUrl: 'https://example.com/image',
        },
      },
      {
        selectedItem: {
          title: 'title2',
          url: 'https://example.com/2',
          imageUrl: 'https://example.com/2/image',
        },
      },
    ];
    tableElement.rows = [
      {
        title: 'foo',
        descriptions: ['foo1', 'foo2'],
        summaries: ['', ''],
      },
    ];
    await waitAfterNextRender(tableElement);
    const summaryElements =
        tableElement.shadowRoot!.querySelectorAll('.row-summary');
    assertEquals(0, summaryElements.length);
  });
});
