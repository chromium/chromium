// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import type {DragManagerDelegate} from 'chrome://tab-strip.top-chrome/drag_manager.js';
import {DragManager, PLACEHOLDER_GROUP_ID, PLACEHOLDER_TAB_ID} from 'chrome://tab-strip.top-chrome/drag_manager.js';
import type {TabElement} from 'chrome://tab-strip.top-chrome/tab.js';
import type {TabGroupElement} from 'chrome://tab-strip.top-chrome/tab_group.js';
import {TabsApiProxyImpl} from 'chrome://tab-strip.top-chrome/tabs_api_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createTab, TestTabsApiProxy} from './test_tabs_api_proxy.js';

class MockDelegate extends HTMLElement implements DragManagerDelegate {
  getIndexOfTab(tabElement: TabElement) {
    return Array.from(this.querySelectorAll('tabstrip-tab'))
        .indexOf(tabElement);
  }

  placeTabElement(
      element: TabElement, index: number, _pinned: boolean,
      groupId: string|null) {
    element.remove();

    const parent =
        groupId ? this.querySelector(`[data-group-id=${groupId}]`) : this;
    parent!.insertBefore(element, this.children[index]!);
  }

  placeTabGroupElement(element: TabGroupElement, index: number) {
    element.remove();
    this.insertBefore(element, this.children[index]!);
  }

  shouldPreventDrag() {
    return false;
  }
}
customElements.define('mock-delegate', MockDelegate);

class MockDataTransfer extends DataTransfer {
  private dropEffect_: 'link'|'none'|'copy'|'move' = 'none';
  private effectAllowed_: 'none'|'copy'|'copyLink'|'copyMove'|'link'|'linkMove'|
      'move'|'all'|'uninitialized' = 'none';
  dragImageData: {image?: Element, offsetX?: number, offsetY?: number};

  constructor() {
    super();

    this.dragImageData = {};
  }

  override get dropEffect() {
    return this.dropEffect_;
  }

  override set dropEffect(effect) {
    this.dropEffect_ = effect;
  }

  override get effectAllowed() {
    return this.effectAllowed_;
  }

  override set effectAllowed(effect) {
    this.effectAllowed_ = effect;
  }

  override setDragImage(image: Element, offsetX: number, offsetY: number) {
    this.dragImageData.image = image;
    this.dragImageData.offsetX = offsetX;
    this.dragImageData.offsetY = offsetY;
  }
}

suite('DragManager', () => {
  let delegate: MockDelegate;
  let dragManager: DragManager;
  let testTabsApiProxy: TestTabsApiProxy;

  const tabs = [
    createTab({
      active: true,
      id: 0,
      index: 0,
      title: 'Tab 1',
    }),
    createTab({
      id: 1,
      index: 1,
      title: 'Tab 2',
    }),
  ];

  const strings = {
    tabGroupIdDataType: 'application/group-id',
    tabIdDataType: 'application/tab-id',
  };

  function groupTab(tabElement: TabElement, groupId: string): TabGroupElement {
    const groupElement = document.createElement('tabstrip-tab-group');
    groupElement.setAttribute('data-group-id', groupId);
    delegate.replaceChild(groupElement, tabElement);

    tabElement.tab = Object.assign({}, tabElement.tab, {groupId});
    groupElement.appendChild(tabElement);
    return groupElement;
  }

  setup(() => {
    loadTimeData.overrideValues(strings);
    testTabsApiProxy = new TestTabsApiProxy();
    TabsApiProxyImpl.setInstance(testTabsApiProxy);

    delegate = new MockDelegate();
    tabs.forEach(tab => {
      const tabElement = document.createElement('tabstrip-tab');
      tabElement.tab = tab;
      delegate.appendChild(tabElement);
    });
    dragManager = new DragManager(delegate);
    dragManager.startObserving();

    document.body.style.margin = '0';
    document.body.appendChild(delegate);
  });

  test('DragStartSetsDragImage', () => {
    const draggedElement = delegate.children[0] as TabElement | TabGroupElement;
    const dragImage = draggedElement.getDragImage();
    const dragImageCenter = draggedElement.getDragImageCenter();

    // Mock the dimensions and position of the element and the drag image.
    const draggedElementRect = {top: 20, left: 30, width: 200, height: 150};
    draggedElement.getBoundingClientRect = () => draggedElementRect as DOMRect;
    const dragImageRect = {top: 20, left: 30, width: 200, height: 150};
    dragImage.getBoundingClientRect = () => dragImageRect as DOMRect;
    const dragImageCenterRect = {top: 25, left: 25, width: 100, height: 120};
    dragImageCenter.getBoundingClientRect = () =>
        dragImageCenterRect as DOMRect;

    const eventClientX = 100;
    const eventClientY = 50;
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: eventClientX,
      clientY: eventClientY,
      dataTransfer: mockDataTransfer,
    });
    draggedElement.dispatchEvent(dragStartEvent);
    assertEquals(dragStartEvent.dataTransfer!.effectAllowed, 'move');
    assertEquals(
        mockDataTransfer.dragImageData.image, draggedElement.getDragImage());

    const eventXPercentage =
        (eventClientX - draggedElementRect.left) / draggedElementRect.width;
    const eventYPercentage =
        (eventClientY - draggedElementRect.top) / draggedElementRect.height;

    // Offset should account for any margins or padding between the
    // dragImageCenter and the dragImage.
    let dragImageCenterLeftMargin =
        dragImageCenterRect.left - dragImageRect.left;
    let dragImageCenterTopMargin = dragImageCenterRect.top - dragImageRect.top;
    if (isChromeOS) {
      // Dimensions are scaled on ChromeOS so the margins and paddings are also
      // scaled.
      dragImageCenterLeftMargin *= 1.2;
      dragImageCenterTopMargin *= 1.2;
    }

    // Offset should map event's coordinates to within the dimensions of the
    // dragImageCenter.
    const eventXWithinDragImageCenter =
        eventXPercentage * dragImageCenterRect.width;
    const eventYWithinDragImageCenter =
        eventYPercentage * dragImageCenterRect.height;

    const expectedOffsetX =
        dragImageCenterLeftMargin + eventXWithinDragImageCenter;
    let expectedOffsetY =
        dragImageCenterTopMargin + eventYWithinDragImageCenter;
    if (isChromeOS) {
      expectedOffsetY -= 25;
    }

    assertEquals(expectedOffsetX, mockDataTransfer.dragImageData.offsetX);
    assertEquals(expectedOffsetY, mockDataTransfer.dragImageData.offsetY);
  });

  test('DragOverMovesTabs', async () => {
    const draggedIndex = 0;
    const dragOverIndex = 1;
    const draggedTab = delegate.children[draggedIndex]!;
    const dragOverTab = delegate.children[dragOverIndex]!;
    const mockDataTransfer = new MockDataTransfer();

    // Dispatch a dragstart event to start the drag process.
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Move the draggedTab over the 2nd tab.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);
    assertEquals(dragOverEvent.dataTransfer!.dropEffect, 'move');

    // Dragover tab and dragged tab have now switched places in the DOM.
    assertEquals(draggedTab, delegate.children[dragOverIndex]);
    assertEquals(dragOverTab, delegate.children[draggedIndex]);

    draggedTab.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [tabId, newIndex] = await testTabsApiProxy.whenCalled('moveTab');
    assertEquals(tabId, tabs[draggedIndex]!.id);
    assertEquals(newIndex, dragOverIndex);
  });

  test('DragTabOverTabGroup', async () => {
    const tabElements = delegate.children as HTMLCollectionOf<TabElement>;

    // Group the first tab.
    const dragOverTabGroup = groupTab(tabElements[0]!, 'group0');

    // Start dragging the second tab.
    const draggedTab = tabElements[1]!;
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Drag the second tab over the newly created tab group.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTabGroup.dispatchEvent(dragOverEvent);

    // Tab is now in the group within the DOM.
    assertEquals(dragOverTabGroup, draggedTab.parentElement);

    draggedTab.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [tabId, groupId] = await testTabsApiProxy.whenCalled('groupTab');
    assertEquals(draggedTab.tab.id, tabId);
    assertEquals('group0', groupId);
  });

  test('DragTabOutOfTabGroup', async () => {
    // Group the first tab.
    const draggedTab = (delegate.children as HTMLCollectionOf<TabElement>)[0]!;
    groupTab(draggedTab, 'group0');

    // Start dragging the first tab.
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Drag the first tab out.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragOverEvent);

    // The tab is now outside of the group in the DOM.
    assertEquals(delegate, draggedTab.parentElement);

    draggedTab.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [tabId] = await testTabsApiProxy.whenCalled('ungroupTab');
    assertEquals(draggedTab.tab.id, tabId);
  });

  test('DragGroupOverTab', async () => {
    const tabElements = delegate.children as HTMLCollectionOf<TabElement>;

    // Start dragging the group.
    const draggedGroupIndex = 0;
    const draggedGroup = groupTab(tabElements[draggedGroupIndex]!, 'group0');
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedGroup.shadowRoot!.getElementById('dragHandle')!.dispatchEvent(
        dragStartEvent);

    // Drag the group over the second tab.
    const dragOverIndex = 1;
    const dragOverTab = tabElements[dragOverIndex]!;
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);

    // Group and tab have now switched places.
    assertEquals(draggedGroup, delegate.children[dragOverIndex]);
    assertEquals(dragOverTab, delegate.children[draggedGroupIndex]);

    draggedGroup.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [groupId, index] = await testTabsApiProxy.whenCalled('moveGroup');
    assertEquals('group0', groupId);
    assertEquals(1, index);
  });

  test('DragGroupOverGroup', async () => {
    const tabElements = delegate.children as HTMLCollectionOf<TabElement>;

    // Group the first tab and second tab separately.
    const draggedIndex = 0;
    const draggedGroup = groupTab(tabElements[draggedIndex]!, 'group0');
    const dragOverIndex = 1;
    const dragOverGroup = groupTab(tabElements[dragOverIndex]!, 'group1');

    // Start dragging the first group.
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedGroup.shadowRoot!.getElementById('dragHandle')!.dispatchEvent(
        dragStartEvent);

    // Drag the group over the second tab.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverGroup.dispatchEvent(dragOverEvent);

    // Groups have now switched places.
    assertEquals(draggedGroup, delegate.children[dragOverIndex]);
    assertEquals(dragOverGroup, delegate.children[draggedIndex]);

    draggedGroup.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [groupId, index] = await testTabsApiProxy.whenCalled('moveGroup');
    assertEquals('group0', groupId);
    assertEquals(1, index);
  });

  test('DragExternalTabOverTab', async () => {
    const externalTabId = 1000;
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabIdDataType, `${externalTabId}`);
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragEnterEvent);

    // Test that a placeholder tab was created.
    const placeholderTabElement = delegate.lastElementChild as TabElement;
    assertEquals(PLACEHOLDER_TAB_ID, placeholderTabElement.tab.id);

    const dragOverIndex = 0;
    const dragOverTab = delegate.children[dragOverIndex]!;
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);
    assertEquals(placeholderTabElement, delegate.children[dragOverIndex]);

    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dropEvent);
    assertEquals(externalTabId, placeholderTabElement.tab.id);
    const [tabId, index] = await testTabsApiProxy.whenCalled('moveTab');
    assertEquals(externalTabId, tabId);
    assertEquals(dragOverIndex, index);
  });

  test('DragExternalTabOverTabGroup', async () => {
    const externalTabId = 1000;
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabIdDataType, `${externalTabId}`);
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragEnterEvent);
    const placeholderTabElement = delegate.lastElementChild!;

    const draggedGroup = groupTab(delegate.children[0] as TabElement, 'group0');
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    draggedGroup.dispatchEvent(dragOverEvent);
    assertEquals(draggedGroup, placeholderTabElement.parentElement);

    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    draggedGroup.dispatchEvent(dropEvent);
    const [tabId, groupId] = await testTabsApiProxy.whenCalled('groupTab');
    assertEquals(externalTabId, tabId);
    assertEquals('group0', groupId);
  });

  test('DragExternalTabGroupOverTab', async () => {
    const externalGroupId = 'external-group';
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabGroupIdDataType, `${externalGroupId}`);
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragEnterEvent);

    // Test that a placeholder group was created.
    const placeholderGroupElement = delegate.lastElementChild as TabElement;
    assertEquals(
        PLACEHOLDER_GROUP_ID, placeholderGroupElement.dataset['groupId']);

    function dragOverTabAt(dragOverIndex: number) {
      const dragOverTab = delegate.children[dragOverIndex]!;
      const dragOverEvent = new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        dataTransfer: mockDataTransfer,
      });
      dragOverTab.dispatchEvent(dragOverEvent);
      assertEquals(placeholderGroupElement, delegate.children[dragOverIndex]);
    }

    // Test moving forwards and backwards in the tab strip.
    dragOverTabAt(0);
    dragOverTabAt(1);
    dragOverTabAt(2);
    dragOverTabAt(0);

    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    placeholderGroupElement.dispatchEvent(dropEvent);
    assertEquals(externalGroupId, placeholderGroupElement.dataset['groupId']);
    const [groupId, index] = await testTabsApiProxy.whenCalled('moveGroup');
    assertEquals(externalGroupId, groupId);
    assertEquals(0, index);
  });

  test('DragExternalTabGroupOverTabGroup', async () => {
    const externalGroupId = 'external-group';
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabGroupIdDataType, `${externalGroupId}`);
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragEnterEvent);
    const placeholderGroupElement = delegate.lastElementChild!;

    const dragOverGroupIndex = 0;
    const dragOverGroup =
        groupTab(delegate.children[dragOverGroupIndex] as TabElement, 'group0');
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverGroup.dispatchEvent(dragOverEvent);
    assertEquals(
        placeholderGroupElement, delegate.children[dragOverGroupIndex]);

    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    placeholderGroupElement.dispatchEvent(dropEvent);
    const [groupId, index] = await testTabsApiProxy.whenCalled('moveGroup');
    assertEquals(externalGroupId, groupId);
    assertEquals(dragOverGroupIndex, index);
  });

  test('CancelDragResetsPosition', () => {
    const draggedIndex = 0;
    const draggedTab = delegate.children[draggedIndex]!;
    const mockDataTransfer = new MockDataTransfer();

    // Dispatch a dragstart event to start the drag process.
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Move the draggedTab over the 2nd tab.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.children[1]!.dispatchEvent(dragOverEvent);

    draggedTab.dispatchEvent(new DragEvent('dragend', {bubbles: true}));
    assertEquals(draggedTab, delegate.children[draggedIndex]);
  });

  test('DragLeaveRemovesExternalTab', () => {
    const externalTabId = 1000;
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabIdDataType, `${externalTabId}`);
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragEnterEvent);
    assertTrue(
        !!delegate.querySelector(`[data-tab-id="${PLACEHOLDER_TAB_ID}"]`));

    const dragLeaveEvent = new DragEvent('dragleave', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragLeaveEvent);
    assertFalse(
        !!delegate.querySelector(`[data-tab-id="${PLACEHOLDER_TAB_ID}"]`));
  });

  test('DragOverInvalidDragOverTarget', () => {
    const draggedIndex = 0;
    const dragOverIndex = 1;
    const draggedTab = delegate.children[draggedIndex]!;
    const dragOverTab = delegate.children[dragOverIndex] as TabElement;
    const mockDataTransfer = new MockDataTransfer();

    // Dispatch a dragstart event to start the drag process.
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Mark the dragOverIndex tab to be an invalid dragover target.
    dragOverTab.isValidDragOverTarget = false;
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);

    // Dragover tab and dragged tab remain in their initial positions.
    assertEquals(draggedTab, delegate.children[draggedIndex]);
    assertEquals(dragOverTab, delegate.children[dragOverIndex]);
  });

  test('DragLeaveUpdatesElementsAsDraggedOut', () => {
    let isDraggedOut = false;

    // Mock a tab's setDraggedOut method to ensure it is called.
    const draggedTab = delegate.children[0] as TabElement;
    draggedTab.setDraggedOut = (isDraggedOutParam) => {
      isDraggedOut = isDraggedOutParam;
    };

    const dataTransfer = new MockDataTransfer();
    draggedTab.dispatchEvent(new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer,
    }));

    delegate.dispatchEvent(new DragEvent('dragleave', {dataTransfer}));
    assertTrue(isDraggedOut);

    delegate.dispatchEvent(new DragEvent('dragover', {dataTransfer}));
    assertFalse(isDraggedOut);
  });

  test('DragendAfterMovingDoesNotShowContextMenu', async () => {
    const draggedTab = delegate.children[0]!;
    const dragOverTab = delegate.children[1]!;
    const dragDetails = {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: new MockDataTransfer(),
    };
    draggedTab.dispatchEvent(new DragEvent('dragstart', dragDetails));
    dragOverTab.dispatchEvent(new DragEvent(
        'dragover', Object.assign({}, dragDetails, {clientX: 200})));
    draggedTab.dispatchEvent(new DragEvent('dragend', dragDetails));

    assertEquals(0, testTabsApiProxy.getCallCount('showTabContextMenu'));
  });

  test('DropPlaceholderWithoutMovingDoesNotShowContextMenu', () => {
    const externalTabId = 1000;
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabIdDataType, `${externalTabId}`);
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragEnterEvent);
    delegate.dispatchEvent(new DragEvent('drop', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    }));
    assertEquals(0, testTabsApiProxy.getCallCount('showTabContextMenu'));
  });

  test('DragEndWithDropEffectMoveDoesNotRemoveDraggedOutAttribute', () => {
    const draggedTab = delegate.children[0] as TabElement;
    const dataTransfer = new MockDataTransfer();
    draggedTab.dispatchEvent(new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer,
    }));
    delegate.dispatchEvent(new DragEvent('dragleave', {dataTransfer}));
    assertTrue(draggedTab.isDraggedOut());

    dataTransfer.dropEffect = 'move';
    delegate.dispatchEvent(new DragEvent('dragend', {dataTransfer}));
    assertTrue(draggedTab.isDraggedOut());
  });

  test('DragEndWithDropEffectNoneRemovesDraggedOutAttribute', () => {
    const draggedTab = delegate.children[0] as TabElement;
    const dataTransfer = new MockDataTransfer();
    draggedTab.dispatchEvent(new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer,
    }));
    delegate.dispatchEvent(new DragEvent('dragleave', {dataTransfer}));
    assertTrue(draggedTab.isDraggedOut());

    dataTransfer.dropEffect = 'none';
    delegate.dispatchEvent(new DragEvent('dragend', {dataTransfer}));
    assertFalse(draggedTab.isDraggedOut());
  });

  test('DragIsPrevented', async () => {
    // Mock the delegate to return true for shouldPreventDrag.
    delegate.shouldPreventDrag = () => true;

    const draggedTab = delegate.children[0]!;
    let isDefaultPrevented = false;
    delegate.addEventListener('dragstart', e => {
      isDefaultPrevented = e.defaultPrevented;
    });

    const dataTransfer = new MockDataTransfer();
    draggedTab.dispatchEvent(new DragEvent('dragstart', {
      bubbles: true,
      cancelable: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer,
    }));
    assertTrue(isDefaultPrevented);
  });
});
