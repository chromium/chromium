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
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {$$, assertNotStyle, assertStyle} from './test_support.js';

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
        productDetails: [
          {
            title: 'foo',
            content: {
              attributes: [{label: '', value: 'bar'}],
              summary: [],
            },
          },
        ],
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

  function dispatchDragLeave({origin}: {origin: HTMLElement}) {
    const event = new DragEvent('dragleave', {});
    event.composedPath = () => [origin];
    tableElement.dispatchEvent(event);
  }

  function assertNotDragging() {
    assertTrue(!tableElement.draggingColumn);
    assertFalse(!!$$(tableElement, '.col[is-dragging'));
  }

  function assertDragging(element: HTMLElement) {
    assertEquals(element, tableElement.draggingColumn);
    assertTrue(!!$$(tableElement, '.col[is-dragging'));
    const dragRgba = 'rgb(255, 0, 0)';
    tableElement.style.setProperty(
        '--color-product-specifications-summary-background-dragging', dragRgba);
    assertStyleOnPseudoElement(
        element, ':before', 'background-color', dragRgba);
  }

  function assertTitleVisible(element: HTMLElement) {
    assertTrue(element.hasAttribute('is-first-column'));
    const title = element.querySelector('.detail-title span');
    assertTrue(!!title);
    assertNotStyle(title!, 'visibility', 'hidden');
  }

  function assertTitleHidden(element: HTMLElement) {
    assertFalse(element.hasAttribute('is-first-column'));
    const title = element.querySelector('.detail-title span');
    assertTrue(!!title);
    assertStyle(title!, 'visibility', 'hidden');
  }

  function assertStyleOnPseudoElement(
      element: HTMLElement, pseudoSelector: string, property: string,
      expected: string) {
    return window.getComputedStyle(element, pseudoSelector)
               .getPropertyValue(property) === expected;
  }

  [true, false].forEach(dropNotLeave => {
    test('drag first column to second position', async () => {
      initializeColumns({numColumns: 2});
      const columns =
          tableElement.$.table.querySelectorAll<HTMLElement>('.col');
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

      const eventPromise = eventToPromise('url-order-update', tableElement);
      if (dropNotLeave) {
        dispatchDrop({origin: first});
      } else {
        dispatchDragLeave({origin: first});
      }
      await waitAfterNextRender(tableElement);
      assertNotDragging();

      const event = await eventPromise;
      assertTrue(!!event);
      assertEquals(2, images.length);
      assertEquals('https://1', images[0]!.autoSrc);
      assertEquals('https://0', images[1]!.autoSrc);
      assertStyle(first, 'order', '0');
      assertStyle(second, 'order', '0');
    });
  });

  [true, false].forEach(dropNotLeave => {
    test('drag second column to first position', async () => {
      initializeColumns({numColumns: 3});
      const columns =
          tableElement.$.table.querySelectorAll<HTMLElement>('.col');
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

      if (dropNotLeave) {
        dispatchDrop({origin: second});
      } else {
        dispatchDragLeave({origin: second});
      }
      await waitAfterNextRender(tableElement);
      assertNotDragging();

      assertEquals('https://1', images[0]!.autoSrc);
      assertEquals('https://0', images[1]!.autoSrc);
      assertEquals('https://2', images[2]!.autoSrc);
      assertStyle(first, 'order', '0');
      assertStyle(second, 'order', '0');
    });
  });

  test('dragover multiple times before dropping', async () => {
    initializeColumns({numColumns: 4});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(4, columns.length);
    assertNotDragging();

    const first = columns[0]!;
    const second = columns[1]!;
    const third = columns[2]!;
    const fourth = columns[3]!;
    dispatchDragStart({origin: first});
    assertDragging(first);
    assertStyle(first, 'order', '0');
    assertStyle(second, 'order', '1');
    assertStyle(third, 'order', '2');
    assertStyle(fourth, 'order', '3');

    dispatchDragOver({target: second});
    assertStyle(first, 'order', '1');
    assertStyle(second, 'order', '0');
    assertStyle(third, 'order', '2');
    assertStyle(fourth, 'order', '3');

    dispatchDragOver({target: third});
    assertStyle(first, 'order', '2');
    assertStyle(second, 'order', '0');
    assertStyle(third, 'order', '1');
    assertStyle(fourth, 'order', '3');

    dispatchDragOver({target: fourth});
    assertStyle(first, 'order', '3');
    assertStyle(second, 'order', '0');
    assertStyle(third, 'order', '1');
    assertStyle(fourth, 'order', '2');

    const images =
        tableElement.$.table.querySelectorAll<CrAutoImgElement>('.col img');
    assertEquals(4, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);
    assertEquals('https://2', images[2]!.autoSrc);
    assertEquals('https://3', images[3]!.autoSrc);

    dispatchDrop({origin: first});
    await waitAfterNextRender(tableElement);
    assertNotDragging();

    assertEquals('https://1', images[0]!.autoSrc);
    assertEquals('https://2', images[1]!.autoSrc);
    assertEquals('https://3', images[2]!.autoSrc);
    assertEquals('https://0', images[3]!.autoSrc);
    assertStyle(first, 'order', '0');
    assertStyle(second, 'order', '0');
    assertStyle(third, 'order', '0');
    assertStyle(fourth, 'order', '0');

  });

  test('swap the same two columns back-to-back', async () => {
    initializeColumns({numColumns: 3});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(3, columns.length);
    assertNotDragging();

    const first = columns[0]!;
    const second = columns[1]!;
    dispatchDragStart({origin: second});
    const images =
        tableElement.$.table.querySelectorAll<CrAutoImgElement>('.col img');
    assertEquals(3, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);
    assertEquals('https://2', images[2]!.autoSrc);

    dispatchDrop({origin: first});

    await waitAfterNextRender(tableElement);
    assertEquals('https://1', images[0]!.autoSrc);
    assertEquals('https://0', images[1]!.autoSrc);
    assertEquals('https://2', images[2]!.autoSrc);

    dispatchDragStart({origin: second});
    dispatchDrop({origin: first});

    await waitAfterNextRender(tableElement);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);
    assertEquals('https://2', images[2]!.autoSrc);
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
    assertStyle(first, 'order', '0');
    assertStyle(second, 'order', '0');
  });

  test('cancel drop after dragover', async () => {
    initializeColumns({numColumns: 2});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(2, columns.length);
    const first = columns[0]!;
    const second = columns[1]!;
    dispatchDragStart({origin: first});
    dispatchDragOver({target: second});
    assertStyle(first, 'order', '1');
    assertStyle(second, 'order', '0');

    document.dispatchEvent(new DragEvent('dragend'));
    await waitAfterNextRender(tableElement);
    assertNotDragging();

    assertStyle(first, 'order', '0');
    assertStyle(second, 'order', '0');
  });

  [true, false].forEach(dropNotEnd => {
    test('titles always show in first column', async () => {
      initializeColumns({numColumns: 3});
      const columns =
          tableElement.$.table.querySelectorAll<HTMLElement>('.col');
      assertEquals(3, columns.length);
      const first = columns[0]!;
      const second = columns[1]!;
      const third = columns[2]!;
      assertTitleVisible(first);
      assertTitleHidden(second);
      assertTitleHidden(third);

      dispatchDragStart({origin: first});

      assertTitleVisible(first);
      assertTitleHidden(second);
      assertTitleHidden(third);

      dispatchDragOver({target: second});

      assertTitleHidden(first);
      assertTitleVisible(second);
      assertTitleHidden(third);

      if (dropNotEnd) {
        dispatchDrop({origin: second});
      } else {
        document.dispatchEvent(new DragEvent('dragend'));
      }
      await waitAfterNextRender(tableElement);

      // Attribute should go back to the first column.
      assertTitleVisible(first);
      assertTitleHidden(second);
      assertTitleHidden(third);
    });
  });
});
