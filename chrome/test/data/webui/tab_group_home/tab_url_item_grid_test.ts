// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-group-home/url_item_grid/url_item_grid.js';

import type {UrlItem, UrlItemDelegate} from 'chrome://tab-group-home/url_item_grid/url_item_delegate.js';
import type {UrlItemGrid} from 'chrome://tab-group-home/url_item_grid/url_item_grid.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
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

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    gridElement = document.createElement('url-item-grid');
    gridElement.setDelegate(new TestUrlItemDelegate(DEFAULT_INITIAL_ITEMS));
    document.body.appendChild(gridElement);
    return microtasksFinished();
  });

  test('grid renders number of items correctly', function() {
    const expectedItemCount = DEFAULT_INITIAL_ITEMS.length;
    const items = gridElement.shadowRoot.querySelectorAll('.item');
    assertEquals(expectedItemCount, items.length);
  });
});
