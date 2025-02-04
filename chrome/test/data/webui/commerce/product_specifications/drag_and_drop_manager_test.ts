// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/app.js';

import {type DragAndDropManager, IS_FIRST_COLUMN_ATTR} from 'chrome://compare/drag_and_drop_manager.js';
import type {TableColumn, TableElement} from 'chrome://compare/table.js';
import type {CrAutoImgElement} from 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, hasStyle, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ProductSpecificationsTableTest', () => {
  let tableElement: TableElement;
  let dragAndDropManager: DragAndDropManager;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    tableElement = document.createElement('product-specifications-table');
    dragAndDropManager = tableElement.getDragAndDropManager();
  });

  teardown(() => {
    dragAndDropManager.destroy();
    return microtasksFinished();
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
    await microtasksFinished();
  }

  function dispatchDragStart({origin}: {origin: HTMLElement}) {
    const event = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
    });
    event.composedPath = () => [origin];
    tableElement.dispatchEvent(event);
    return microtasksFinished();
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
    return microtasksFinished();
  }

  function dispatchDrop({origin}: {origin: HTMLElement}) {
    const event = new DragEvent('drop', {});
    event.composedPath = () => [origin];
    document.dispatchEvent(event);
    return microtasksFinished();
  }

  function dispatchDragLeave({origin}: {origin: HTMLElement}) {
    const event = new DragEvent('dragleave', {});
    event.composedPath = () => [origin];
    tableElement.dispatchEvent(event);
    return microtasksFinished();
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
    assertTrue(element.hasAttribute(IS_FIRST_COLUMN_ATTR));
    const title = element.querySelector('.detail-title span');
    assertTrue(!!title);
    assertFalse(hasStyle(title, 'visibility', 'hidden'));
  }

  function assertTitleHidden(element: HTMLElement) {
    assertFalse(element.hasAttribute(IS_FIRST_COLUMN_ATTR));
    const title = element.querySelector('.detail-title span');
    assertTrue(!!title);
    assertTrue(hasStyle(title, 'visibility', 'hidden'));
  }

  function assertStyleOnPseudoElement(
      element: HTMLElement, pseudoSelector: string, property: string,
      expected: string) {
    return window.getComputedStyle(element, pseudoSelector)
               .getPropertyValue(property) === expected;
  }

  [true, false].forEach(dropNotLeave => {
    test(
        `drag first column to second position - dropNotLeave=${dropNotLeave}`,
        async () => {
          initializeColumns({numColumns: 2});
          const columns =
              tableElement.$.table.querySelectorAll<HTMLElement>('.col');
          assertEquals(2, columns.length);
          assertNotDragging();

          const first = columns[0]!;
          const second = columns[1]!;
          await dispatchDragStart({origin: first});
          assertDragging(first);
          assertTrue(hasStyle(first, 'order', '0'));
          assertTrue(hasStyle(second, 'order', '1'));

          await dispatchDragOver({target: second});
          assertTrue(hasStyle(first, 'order', '1'));
          assertTrue(hasStyle(second, 'order', '0'));

          const images =
              tableElement.$.table.querySelectorAll<CrAutoImgElement>(
                  '.col img[is=cr-auto-img]');
          assertEquals(2, images.length);
          assertEquals('https://0', images[0]!.autoSrc);
          assertEquals('https://1', images[1]!.autoSrc);

          const eventPromise = eventToPromise('url-order-update', tableElement);
          if (dropNotLeave) {
            await dispatchDrop({origin: first});
          } else {
            await dispatchDragLeave({origin: first});
          }
          assertNotDragging();

          await eventPromise;
          assertEquals(2, images.length);
          assertEquals('https://1', images[0]!.autoSrc);
          assertEquals('https://0', images[1]!.autoSrc);
          assertTrue(hasStyle(first, 'order', '0'));
          assertTrue(hasStyle(second, 'order', '0'));
        });
  });

  [true, false].forEach(dropNotLeave => {
    test(
        `drag second column to first position - dropNotLeave=${dropNotLeave}`,
        async () => {
          initializeColumns({numColumns: 3});
          const columns =
              tableElement.$.table.querySelectorAll<HTMLElement>('.col');
          assertEquals(3, columns.length);
          assertNotDragging();

          const first = columns[0]!;
          const second = columns[1]!;
          await dispatchDragStart({origin: second});
          assertDragging(second);
          assertTrue(hasStyle(first, 'order', '0'));
          assertTrue(hasStyle(second, 'order', '1'));

          await dispatchDragOver({target: first});
          assertTrue(hasStyle(first, 'order', '1'));
          assertTrue(hasStyle(second, 'order', '0'));

          const images =
              tableElement.$.table.querySelectorAll<CrAutoImgElement>(
                  '.col img[is=cr-auto-img]');
          assertEquals(3, images.length);
          assertEquals('https://0', images[0]!.autoSrc);
          assertEquals('https://1', images[1]!.autoSrc);
          assertEquals('https://2', images[2]!.autoSrc);

          if (dropNotLeave) {
            await dispatchDrop({origin: second});
          } else {
            await dispatchDragLeave({origin: second});
          }
          await microtasksFinished();
          assertNotDragging();

          assertEquals('https://1', images[0]!.autoSrc);
          assertEquals('https://0', images[1]!.autoSrc);
          assertEquals('https://2', images[2]!.autoSrc);
          assertTrue(hasStyle(first, 'order', '0'));
          assertTrue(hasStyle(second, 'order', '0'));
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
    await dispatchDragStart({origin: first});
    assertDragging(first);
    assertTrue(hasStyle(first, 'order', '0'));
    assertTrue(hasStyle(second, 'order', '1'));
    assertTrue(hasStyle(third, 'order', '2'));
    assertTrue(hasStyle(fourth, 'order', '3'));

    await dispatchDragOver({target: second});
    assertTrue(hasStyle(first, 'order', '1'));
    assertTrue(hasStyle(second, 'order', '0'));
    assertTrue(hasStyle(third, 'order', '2'));
    assertTrue(hasStyle(fourth, 'order', '3'));

    await dispatchDragOver({target: third});
    assertTrue(hasStyle(first, 'order', '2'));
    assertTrue(hasStyle(second, 'order', '0'));
    assertTrue(hasStyle(third, 'order', '1'));
    assertTrue(hasStyle(fourth, 'order', '3'));

    await dispatchDragOver({target: fourth});
    assertTrue(hasStyle(first, 'order', '3'));
    assertTrue(hasStyle(second, 'order', '0'));
    assertTrue(hasStyle(third, 'order', '1'));
    assertTrue(hasStyle(fourth, 'order', '2'));

    const images = tableElement.$.table.querySelectorAll<CrAutoImgElement>(
        '.col img[is=cr-auto-img]');
    assertEquals(4, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);
    assertEquals('https://2', images[2]!.autoSrc);
    assertEquals('https://3', images[3]!.autoSrc);

    await dispatchDrop({origin: first});
    await microtasksFinished();
    assertNotDragging();

    assertEquals('https://1', images[0]!.autoSrc);
    assertEquals('https://2', images[1]!.autoSrc);
    assertEquals('https://3', images[2]!.autoSrc);
    assertEquals('https://0', images[3]!.autoSrc);
    assertTrue(hasStyle(first, 'order', '0'));
    assertTrue(hasStyle(second, 'order', '0'));
    assertTrue(hasStyle(third, 'order', '0'));
    assertTrue(hasStyle(fourth, 'order', '0'));

  });

  test('swap the same two columns back-to-back', async () => {
    initializeColumns({numColumns: 3});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(3, columns.length);
    assertNotDragging();

    const first = columns[0]!;
    const second = columns[1]!;
    await dispatchDragStart({origin: second});
    const images = tableElement.$.table.querySelectorAll<CrAutoImgElement>(
        '.col img[is=cr-auto-img]');
    assertEquals(3, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);
    assertEquals('https://2', images[2]!.autoSrc);

    await dispatchDrop({origin: first});

    await microtasksFinished();
    assertEquals('https://1', images[0]!.autoSrc);
    assertEquals('https://0', images[1]!.autoSrc);
    assertEquals('https://2', images[2]!.autoSrc);

    await dispatchDragStart({origin: second});
    await dispatchDrop({origin: first});

    await microtasksFinished();
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
    await dispatchDragStart({origin: first});
    assertDragging(first);
    assertTrue(hasStyle(first, 'order', '0'));
    assertTrue(hasStyle(second, 'order', '1'));

    const images = tableElement.$.table.querySelectorAll<CrAutoImgElement>(
        '.col img[is=cr-auto-img]');
    assertEquals(2, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);

    await dispatchDrop({origin: first});
    await microtasksFinished();
    assertNotDragging();

    assertEquals(2, images.length);
    assertEquals('https://0', images[0]!.autoSrc);
    assertEquals('https://1', images[1]!.autoSrc);
    assertTrue(hasStyle(first, 'order', '0'));
    assertTrue(hasStyle(second, 'order', '0'));
  });

  test('cancel drop after dragover', async () => {
    initializeColumns({numColumns: 2});
    const columns = tableElement.$.table.querySelectorAll<HTMLElement>('.col');
    assertEquals(2, columns.length);
    const first = columns[0]!;
    const second = columns[1]!;
    await dispatchDragStart({origin: first});
    await dispatchDragOver({target: second});
    assertTrue(hasStyle(first, 'order', '1'));
    assertTrue(hasStyle(second, 'order', '0'));

    document.dispatchEvent(new DragEvent('dragend'));
    await microtasksFinished();
    assertNotDragging();

    assertTrue(hasStyle(first, 'order', '0'));
    assertTrue(hasStyle(second, 'order', '0'));
  });

  [true, false].forEach(dropNotEnd => {
    test(
        `titles always show in first column - dropNotEnd=${dropNotEnd}`,
        async () => {
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

          await dispatchDragStart({origin: first});

          assertTitleVisible(first);
          assertTitleHidden(second);
          assertTitleHidden(third);

          await dispatchDragOver({target: second});

          assertTitleHidden(first);
          assertTitleVisible(second);
          assertTitleHidden(third);

          if (dropNotEnd) {
            await dispatchDrop({origin: second});
          } else {
            document.dispatchEvent(new DragEvent('dragend'));
          }
          await microtasksFinished();

          // Attribute should go back to the first column.
          assertTitleVisible(first);
          assertTitleHidden(second);
          assertTitleHidden(third);
        });
  });
});
