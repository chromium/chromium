// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-group-home/url_item_grid/url_item_grid.js';

import {ItemEventType} from 'chrome://tab-group-home/url_item_grid/url_item_delegate.js';
import type {UrlItem, UrlItemDelegate} from 'chrome://tab-group-home/url_item_grid/url_item_delegate.js';
import type {UrlItemGrid} from 'chrome://tab-group-home/url_item_grid/url_item_grid.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

export class TestUrlItemDelegate extends TestBrowserProxy implements
    UrlItemDelegate {
  private items_: UrlItem[] = [];
  private eventTarget_: EventTarget = new EventTarget();

  constructor(initialItems: UrlItem[]) {
    super([
      'getItems',
      'clickItem',
      'removeItems',
      'moveItem',
    ]);

    this.items_ = initialItems.slice();
  }

  getItems(): Promise<UrlItem[]> {
    this.methodCalled('getItems');
    return Promise.resolve(this.items_.slice());
  }

  clickItem(id: number) {
    this.methodCalled('clickItem', id);
  }

  removeItems(ids: number[]) {
    this.methodCalled('removeItems', ids);
  }

  moveItem(id: number, index: number) {
    this.methodCalled('moveItem', id, index);
  }

  getEventTarget(): EventTarget {
    return this.eventTarget_;
  }
}

suite('GridRendering', function() {
  // Define initialItems here so it's accessible to all test cases
  const DEFAULT_INITIAL_ITEMS: UrlItem[] = [
    {id: 0, title: 'Google', url: {url: 'https://www.google.com'}},
    {id: 1, title: 'Wikipedia', url: {url: 'https://www.wikipedia.org'}},
    {id: 2, title: 'Lit Documentation', url: {url: 'https://lit.dev'}},
  ];

  let gridElement: UrlItemGrid;
  let delegate: TestUrlItemDelegate;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    gridElement = document.createElement('url-item-grid');
    delegate = new TestUrlItemDelegate(DEFAULT_INITIAL_ITEMS);
    gridElement.setDelegate(delegate);
    document.body.appendChild(gridElement);
    return microtasksFinished();
  });

  test('grid renders number of items correctly', function() {
    const expectedItemCount = DEFAULT_INITIAL_ITEMS.length;
    const items = gridElement.shadowRoot.querySelectorAll('.item');
    assertEquals(expectedItemCount, items.length);
  });

  test('grid renders newly added item at correct index', async function() {
    const newItem:
        UrlItem = {id: 3, title: 'example', url: {url: 'https://example.com'}};
    const newIndex = 1;

    // Simulate the delegate dispatching an ITEM_ADDED event. The grid component
    // should be listening for this and update its view.
    delegate.getEventTarget().dispatchEvent(new CustomEvent(
        ItemEventType.ITEM_ADDED, {detail: {item: newItem, index: newIndex}}));
    await microtasksFinished();

    // Verify the item count has increased.
    const expectedItemCount = DEFAULT_INITIAL_ITEMS.length + 1;
    const items = gridElement.shadowRoot.querySelectorAll('.item');
    assertEquals(
        expectedItemCount, items.length, 'items length should be equal');

    // Verify the new order.
    const expectedOrder: number[] = [
      DEFAULT_INITIAL_ITEMS[0]!.id,
      newItem.id,
      DEFAULT_INITIAL_ITEMS[1]!.id,
      DEFAULT_INITIAL_ITEMS[2]!.id,
    ];

    assertDeepEquals(
        expectedOrder.map(id => String(id)),
        Array.from(items).map(item => item.id));
  });

  test('grid moves item correctly', async function() {
    const itemToMove = DEFAULT_INITIAL_ITEMS[0]!;
    const newIndex = 1;

    // Simulate the delegate dispatching an ITEM_MOVED event.
    delegate.getEventTarget().dispatchEvent(new CustomEvent(
        ItemEventType.ITEM_MOVED, {detail: {id: itemToMove.id, newIndex}}));
    await microtasksFinished();

    const items = gridElement.shadowRoot.querySelectorAll('.item');
    assertEquals(
        DEFAULT_INITIAL_ITEMS.length, items.length,
        'items length should be equal');

    // Verify the new order.
    const expectedOrder: number[] = [
      DEFAULT_INITIAL_ITEMS[1]!.id,
      DEFAULT_INITIAL_ITEMS[0]!.id,
      DEFAULT_INITIAL_ITEMS[2]!.id,
    ];
    assertDeepEquals(
        expectedOrder.map(id => String(id)),
        Array.from(items).map(item => item.id));
  });

  test('grid removes item correctly', async function() {
    const initialItemCount = DEFAULT_INITIAL_ITEMS.length;
    const itemToRemove = DEFAULT_INITIAL_ITEMS[1]!;

    // Verify initial state.
    let items = gridElement.shadowRoot.querySelectorAll('.item');
    assertEquals(initialItemCount, items.length);

    // Simulate the delegate dispatching an ITEM_REMOVED event.
    delegate.getEventTarget().dispatchEvent(
        new CustomEvent(ItemEventType.ITEM_REMOVED, {detail: itemToRemove.id}));
    await microtasksFinished();

    // Verify the item count has decreased.
    items = gridElement.shadowRoot.querySelectorAll('.item');
    assertEquals(
        initialItemCount - 1, items.length, 'items length should be equal');

    // Verify the new order.
    const expectedOrder: number[] = [
      DEFAULT_INITIAL_ITEMS[0]!.id,
      DEFAULT_INITIAL_ITEMS[2]!.id,
    ];
    assertDeepEquals(
        expectedOrder.map(id => String(id)),
        Array.from(items).map(item => item.id));
  });
});
