// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/drag_and_drop_manager.js';

import type {TableColumn} from 'chrome://compare/app.js';
import {DragAndDropManager} from 'chrome://compare/drag_and_drop_manager.js';
import {TableElement} from 'chrome://compare/table.js';
import type {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {$$, assertStyle} from './test_support.js';

suite('ProductSpecificationsTableTest', () => {
  let tableElement: TableElement;
  let dragAndDropManager: DragAndDropManager;

  setup(async () => {
    dragAndDropManager = new DragAndDropManager();
    tableElement = new TableElement();
  });

  teardown(async () => {
    if (dragAndDropManager) {
      dragAndDropManager.destroy();
      await flushTasks();
    }

    if (tableElement) {
      tableElement.remove();
      await flushTasks();
    }
  });

  async function initializeColumns({numColumns}: {numColumns: number}) {
    const columns: TableColumn[] = [];
    for (let i = 0; i < numColumns; i++) {
      columns.push({
        selectedItem: {
          title: `${i}`,
          url: `https://${i}`,
          imageUrl: `https://${i}`,
        },
        productDetails: [],
      });
    }
    tableElement.columns = columns;
    document.body.appendChild(tableElement);
    await flushTasks();
    dragAndDropManager.init(tableElement);
  }

  function dispatchDragStart({origin}: {origin: HTMLElement}) {
    const event = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
    });
    event.composedPath = () => [origin];
    tableElement.dispatchEvent(event);
  }

  function dispatchDragOver({target}: {target: HTMLElement}) {
    const rect = target.getBoundingClientRect();
    const event = new DragEvent('dragover', {
      clientX: rect.x,
      clientY: rect.y,
      dataTransfer: new DataTransfer(),
    });
    event.composedPath = () => [target];
    document.dispatchEvent(event);
  }

  function dispatchDrop({origin}: {origin: HTMLElement}) {
    const event = new DragEvent('drop', {});
    event.composedPath = () => [origin];
    document.dispatchEvent(event);
  }

  function assertNotDragging() {
    assertTrue(!tableElement || !tableElement.draggingColumn);
    assertFalse(!!$$(tableElement, '.col[is-dragging'));
  }

  function assertDragging(element: HTMLElement) {
    assertEquals(element, tableElement.draggingColumn);
    assertTrue(!!$$(tableElement, '.col[is-dragging'));
    const dragRgba = 'rgb(255, 0, 0)';
    tableElement.style.setProperty(
        '--color-product-specifications-summary-background-dragging', dragRgba);
    assertStyle(element, 'background-color', dragRgba);
  }

  test('drag first column to second position', async () => {
    initializeColumns({numColumns: 2});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(2, columns.length);
    assertNotDragging();

    const first = columns[0]!;
    const second = columns[1]!;
    dispatchDragStart({origin: first});
    assertDragging(first);
    assertStyle(first, 'order', '0');
    assertStyle(second, 'order', '1');

    dispatchDragOver({target: second});
    assertStyle(first, 'order', '1');
    assertStyle(second, 'order', '0');

    const images =
        tableElement.$.table.querySelectorAll<CrAutoImgElement>('.col img');
    assertEquals(2, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);

    dispatchDrop({origin: first});
    await waitAfterNextRender(tableElement);
    assertNotDragging();

    assertEquals(2, images.length);
    assertEquals('https://1', images[0]!.autoSrc);
    assertEquals('https://0', images[1]!.autoSrc);
  });

  test('drag second column to first position', async () => {
    initializeColumns({numColumns: 3});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(3, columns.length);
    assertNotDragging();

    const first = columns[0]!;
    const second = columns[1]!;
    dispatchDragStart({origin: second});
    assertDragging(second);
    assertStyle(first, 'order', '0');
    assertStyle(second, 'order', '1');

    dispatchDragOver({target: first});
    assertStyle(first, 'order', '1');
    assertStyle(second, 'order', '0');

    const images =
        tableElement.$.table.querySelectorAll<CrAutoImgElement>('.col img');
    assertEquals(3, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);
    assertEquals('https://2', images[2]!.autoSrc);

    dispatchDrop({origin: second});
    await waitAfterNextRender(tableElement);
    assertNotDragging();

    assertEquals('https://1', images[0]!.autoSrc);
    assertEquals('https://0', images[1]!.autoSrc);
    assertEquals('https://2', images[2]!.autoSrc);
  });

  test('perform the same dragover back-to-back', async () => {
    initializeColumns({numColumns: 2});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(2, columns.length);
    assertNotDragging();

    const first = columns[0]!;
    const second = columns[1]!;
    dispatchDragStart({origin: first});
    assertDragging(first);
    assertStyle(first, 'order', '0');
    assertStyle(second, 'order', '1');

    dispatchDragOver({target: second});
    assertDragging(first);
    assertStyle(first, 'order', '1');
    assertStyle(second, 'order', '0');

    dispatchDragOver({target: second});
    assertDragging(first);
    assertStyle(first, 'order', '1');
    assertStyle(second, 'order', '0');
  });

  test('leave table mid-drag', async () => {
    initializeColumns({numColumns: 2});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(2, columns.length);
    assertNotDragging();

    const first = columns[0]!;
    dispatchDragStart({origin: first});
    assertDragging(first);
    tableElement.dispatchEvent(
        new DragEvent('dragleave', {bubbles: true, cancelable: true}));

    assertNotDragging();
  });

  test('drop column without dragging over', async () => {
    initializeColumns({numColumns: 2});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(2, columns.length);
    assertNotDragging();

    const first = columns[0]!;
    const second = columns[1]!;
    dispatchDragStart({origin: first});
    assertDragging(first);
    assertStyle(first, 'order', '0');
    assertStyle(second, 'order', '1');

    const images =
        tableElement.$.table.querySelectorAll<CrAutoImgElement>('.col img');
    assertEquals(2, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);

    dispatchDrop({origin: first});
    await waitAfterNextRender(tableElement);
    assertNotDragging();

    assertEquals(2, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);
  });
});
