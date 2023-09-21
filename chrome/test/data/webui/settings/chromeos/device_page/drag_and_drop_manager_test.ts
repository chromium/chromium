// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomizeButtonRowElement, DragAndDropManager, getDataTransferOriginIndex, setDataTransferOriginIndex} from 'chrome://os-settings/lazy_load.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

interface Position {
  x: number;
  y: number;
}

suite('DragAndDropManager test', () => {
  let listContainer: HTMLDivElement;
  let listElements: CustomizeButtonRowElement[];
  let dragAndDropManager: DragAndDropManager;
  let numOnDropCallbackCalls: number;
  let lastOnDropCallbackValues: {originIndex: number, destinationIndex: number}|
      undefined;

  setup(() => {
    dragAndDropManager = new DragAndDropManager();
    numOnDropCallbackCalls = 0;
    lastOnDropCallbackValues = undefined;
  });

  teardown(async () => {
    if (dragAndDropManager) {
      dragAndDropManager.destroy();
    }

    if (listContainer) {
      listContainer.remove();
    }
  });

  // Create the list of CustomizeButtonRowElements that will be used
  // in the tests.
  async function initializeList(
      {numberOfElements}: {numberOfElements: number}) {
    listContainer = document.createElement('div');
    listContainer.id = 'list-container';

    listElements = [];
    for (let i = 0; i < numberOfElements; i++) {
      const newListElement: CustomizeButtonRowElement =
          document.createElement(CustomizeButtonRowElement.is);
      newListElement.id = `list-item-${i}`;
      newListElement.remappingIndex = i;
      // Manually set the height and width to ensure that there's a draggable
      // area in each element.
      newListElement.style.height = '20px';
      newListElement.style.width = '100px';
      listElements.push(newListElement);
      listContainer.appendChild(newListElement);
    }

    document.body.appendChild(listContainer);
    await flushTasks();
    dragAndDropManager.init(listContainer, onDropCallback);
  }

  function onDropCallback(originIndex: number, destinationIndex: number) {
    numOnDropCallbackCalls++;
    lastOnDropCallbackValues = {originIndex, destinationIndex};
  }

  function dispatchDragEventOnList(
      type: string, target: HTMLElement, clientPosition: Position,
      eventData?: object) {
    const props: DragEventInit = {
      bubbles: true,
      cancelable: true,
      composed: true,
      clientX: clientPosition.x,
      clientY: clientPosition.y,
      // Make this a primary input.
      buttons: 1,
      dataTransfer: new DataTransfer(),
    };
    const event = new DragEvent(type, props);
    if (eventData) {
      event.dataTransfer?.setData('settings-data', JSON.stringify(eventData));
    }
    // Override the composedPath function to return the target element since
    // we can't simulate this in the test.
    event.composedPath = () => [target];
    listContainer.dispatchEvent(event);
  }

  function topThirdOfNode(node: HTMLElement) {
    const rect = node.getBoundingClientRect();
    return {y: rect.top + (0.33 * rect.height), x: rect.left + rect.width / 2};
  }

  function bottomThirdOfNode(node: HTMLElement) {
    const rect = node.getBoundingClientRect();
    return {y: rect.top + (0.66 * rect.height), x: rect.left + rect.width / 2};
  }

  function simulateDragOver(
      {overElement, dragPosition}:
          {overElement: HTMLElement, dragPosition: Position}) {
    dispatchDragEventOnList('dragover', overElement, dragPosition);
  }

  function simulateDrop({overElement, dropPosition, indexOfOriginElement}: {
    overElement: HTMLElement,
    dropPosition: Position,
    indexOfOriginElement: number,
  }) {
    dispatchDragEventOnList(
        'drop', overElement, dropPosition, {originIndex: indexOfOriginElement});
  }

  function simulateDragAndDrop(
      {indexOfOriginElement, overElement, dropPositionFunction}: {
        indexOfOriginElement: number,
        overElement: HTMLElement,
        dropPositionFunction: (overElement: HTMLElement) => Position,
      }) {
    const numberOfCallsBeforeDragOver = numOnDropCallbackCalls;
    const dropPosition = dropPositionFunction(overElement);

    // Simulate the drag over so that the DragAndDropManager can get the
    // position of the drop location.
    simulateDragOver({overElement, dragPosition: dropPosition});

    // Assert that the onDrop function was not called as a result of dragging
    // the element over.
    assertEquals(numberOfCallsBeforeDragOver, numOnDropCallbackCalls);

    simulateDrop({overElement, dropPosition, indexOfOriginElement});
  }

  test('Drag first item below next element', async () => {
    await initializeList({numberOfElements: 3});

    simulateDragAndDrop({
      indexOfOriginElement: 0,
      overElement: listElements[1]!,
      dropPositionFunction: bottomThirdOfNode,
    });

    assertEquals(1, numOnDropCallbackCalls);
    assertTrue(!!lastOnDropCallbackValues);
    assertEquals(0, lastOnDropCallbackValues.originIndex);
    assertEquals(1, lastOnDropCallbackValues.destinationIndex);
  });

  test('Drag first item two elements down', async () => {
    await initializeList({numberOfElements: 3});

    simulateDragAndDrop({
      indexOfOriginElement: 0,
      overElement: listElements[2]!,
      dropPositionFunction: bottomThirdOfNode,
    });

    assertEquals(1, numOnDropCallbackCalls);
    assertTrue(!!lastOnDropCallbackValues);
    assertEquals(0, lastOnDropCallbackValues.originIndex);
    assertEquals(2, lastOnDropCallbackValues.destinationIndex);
  });

  test('Drag first item above next element', async () => {
    await initializeList({numberOfElements: 3});

    simulateDragAndDrop({
      indexOfOriginElement: 0,
      overElement: listElements[1]!,
      dropPositionFunction: topThirdOfNode,
    });

    // We expect that the onDrop callback was not called, because we're dropping
    // the first item into the same spot (above the second item).
    assertEquals(0, numOnDropCallbackCalls);
  });

  test('Drag first item above and below itself', async () => {
    await initializeList({numberOfElements: 3});

    simulateDragAndDrop({
      indexOfOriginElement: 0,
      overElement: listElements[0]!,
      dropPositionFunction: topThirdOfNode,
    });
    // We expect that the onDrop callback was not called, because we're dropping
    // the first item into the same spot (above/below itself).
    assertEquals(0, numOnDropCallbackCalls);

    simulateDragAndDrop({
      indexOfOriginElement: 0,
      overElement: listElements[0]!,
      dropPositionFunction: bottomThirdOfNode,
    });
    assertEquals(0, numOnDropCallbackCalls);
  });

  test('Drag third item to top', async () => {
    await initializeList({numberOfElements: 3});

    simulateDragAndDrop({
      indexOfOriginElement: 2,
      overElement: listElements[0]!,
      dropPositionFunction: topThirdOfNode,
    });

    assertEquals(1, numOnDropCallbackCalls);
    assertTrue(!!lastOnDropCallbackValues);
    assertEquals(2, lastOnDropCallbackValues.originIndex);
    assertEquals(0, lastOnDropCallbackValues.destinationIndex);
  });

  test('Drag third item above and below itself', async () => {
    await initializeList({numberOfElements: 3});

    simulateDragAndDrop({
      indexOfOriginElement: 2,
      overElement: listElements[2]!,
      dropPositionFunction: topThirdOfNode,
    });
    // We expect that the onDrop callback was not called, because we're dropping
    // the third item into the same spot.
    assertEquals(0, numOnDropCallbackCalls);

    simulateDragAndDrop({
      indexOfOriginElement: 2,
      overElement: listElements[2]!,
      dropPositionFunction: bottomThirdOfNode,
    });
    assertEquals(0, numOnDropCallbackCalls);
  });

  test('Swap items in two element list', async () => {
    await initializeList({numberOfElements: 2});

    simulateDragAndDrop({
      indexOfOriginElement: 0,
      overElement: listElements[1]!,
      dropPositionFunction: bottomThirdOfNode,
    });

    assertEquals(1, numOnDropCallbackCalls);
    assertTrue(!!lastOnDropCallbackValues);
    assertEquals(0, lastOnDropCallbackValues.originIndex);
    assertEquals(1, lastOnDropCallbackValues.destinationIndex);

    // Swap the second element back to the top.
    simulateDragAndDrop({
      indexOfOriginElement: 1,
      overElement: listElements[0]!,
      dropPositionFunction: topThirdOfNode,
    });

    assertEquals(2, numOnDropCallbackCalls);
    assertTrue(!!lastOnDropCallbackValues);
    assertEquals(1, lastOnDropCallbackValues.originIndex);
    assertEquals(0, lastOnDropCallbackValues.destinationIndex);
  });

  test('DataTransfer set/get', async () => {
    const dragEventProps: DragEventInit = {
      bubbles: true,
      cancelable: true,
      composed: true,
      clientX: 0,
      clientY: 0,
      buttons: 1,
      dataTransfer: new DataTransfer(),
    };
    const dragEvent = new DragEvent('drop', dragEventProps);
    const originIndex = 10;
    setDataTransferOriginIndex(dragEvent, originIndex);
    const dataTransferOriginIndex = getDataTransferOriginIndex(dragEvent);
    assertEquals(originIndex, dataTransferOriginIndex);
  });
});
