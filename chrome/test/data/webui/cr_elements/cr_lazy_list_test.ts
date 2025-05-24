// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';

import type {CrLazyListElement} from 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {CrLitElement, css, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

const SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT = 6;
const SAMPLE_ITEM_HEIGHT = 56;
const SAMPLE_AVAIL_HEIGHT =
    SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT * SAMPLE_ITEM_HEIGHT;

class TestItem extends CrLitElement {
  static get is() {
    return 'test-item';
  }

  static override get properties() {
    return {
      name: {
        type: String,
        reflect: true,
      },
    };
  }

  override render() {
    return html`
<div style="height: 48px; padding: 4px;">
  <span>${this.name}</span>
  <button>click item</button>
</div>`;
  }

  override focus() {
    this.shadowRoot.querySelector('button')!.focus();
  }

  accessor name: string = '';
}

customElements.define('test-item', TestItem);

class TestApp extends CrLitElement {
  static get is() {
    return 'test-app';
  }

  static override get properties() {
    return {
      chunkSize: {type: Number},
      listItems: {type: Array},
      restoreFocusElement_: {type: Object},
      scrollOffset: {type: Number},
    };
  }

  accessor chunkSize: number = 0;
  accessor listItems: Array<{name: string}> = [];
  accessor scrollOffset: number = 0;
  private accessor restoreFocusElement_: HTMLElement|null = null;

  override render() {
    return html`
    <div style="height: ${this.scrollOffset}px"></div>
    <cr-lazy-list .items="${this.listItems}" .scrollTarget="${this}"
        .scrollOffset="${this.scrollOffset}" .chunkSize="${this.chunkSize}"
        .restoreFocusElement="${this.restoreFocusElement_}"
        .template=${(item: {name: string}, idx: number) => html`
            <test-item name="${item.name}"
                id="item-${idx}">
            </test-item>
          `}
        @viewport-filled="${this.onRenderedItemsChanged_}">
    </lazy-list>`;
  }

  private onRenderedItemsChanged_() {
    this.restoreFocusElement_ = this.shadowRoot.querySelector('[name="Two"]');
  }
}

customElements.define('test-app', TestApp);

class TestDocumentTargetApp extends CrLitElement {
  static get is() {
    return 'test-document-target-app';
  }

  static override get properties() {
    return {
      listItems: {type: Array},
      scrollOffset: {type: Number},
    };
  }

  accessor listItems: Array<{name: string}> = [];

  override render() {
    return html`
    <cr-lazy-list .items="${this.listItems}"
        .template=${(item: {name: string}, idx: number) => html`
            <test-item name="${item.name}"
                id="item-${idx}">
            </test-item>
          `}>
    </lazy-list>`;
  }
}

customElements.define('test-document-target-app', TestDocumentTargetApp);

class TestListPaddingApp extends CrLitElement {
  static get is() {
    return 'test-list-padding-app';
  }

  static override get properties() {
    return {
      chunkSize: {type: Number},
      listItems: {type: Array},
    };
  }

  accessor chunkSize: number = 0;
  accessor listItems: Array<{name: string}> = [];

  override render() {
    return html`
    <cr-lazy-list
        style="padding: 16px;" item-size="${SAMPLE_ITEM_HEIGHT}"
        chunk-size="${this.chunkSize}"
        .items="${this.listItems}" .scrollTarget="${this}"
        .template=${(item: {name: string}, idx: number) => html`
            <test-item name="${item.name}"
                id="item-${idx}">
            </test-item>
          `}>
    </lazy-list>`;
  }
}

customElements.define('test-list-padding-app', TestListPaddingApp);

suite('CrLazyListTest', () => {
  let lazyList: CrLazyListElement;
  let testApp: TestApp;

  async function setupTest(
      sampleData: Array<{name: string}>, scrollOffset: number = 0,
      chunkSize: number = 0) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testApp = document.createElement('test-app') as TestApp;
    testApp.style.height = `${SAMPLE_AVAIL_HEIGHT}px`;
    testApp.style.maxHeight = `${SAMPLE_AVAIL_HEIGHT}px`;
    testApp.style.display = 'block';
    testApp.style.overflowY = 'auto';
    testApp.style.overflowX = 'hidden';
    document.body.appendChild(testApp);
    lazyList = testApp.shadowRoot.querySelector('cr-lazy-list')!;
    assertTrue(!!lazyList);
    const listFilled = eventToPromise('viewport-filled', lazyList);
    testApp.chunkSize = chunkSize;
    testApp.listItems = sampleData;
    testApp.scrollOffset = scrollOffset;

    await listFilled;
    await microtasksFinished();
  }

  function queryItems(): NodeListOf<TestItem> {
    return lazyList.querySelectorAll<TestItem>('test-item');
  }

  function getTestItems(count: number): Array<{name: string}> {
    const items = [
      {name: 'One'},
      {name: 'Two'},
      {name: 'Three'},
      {name: 'Four'},
      {name: 'Five'},
      {name: 'Six'},
      {name: 'Seven'},
      {name: 'Eight'},
      {name: 'Nine'},
      {name: 'Ten'},
      {name: 'Eleven'},
      {name: 'Twelve'},
    ];
    const returnVal = [];
    while (returnVal.length < count) {
      const toAdd = Math.min(items.length, count - returnVal.length);
      returnVal.push(...items.slice(0, toAdd));
    }
    return returnVal;
  }

  test('Populates template parameters correctly', async () => {
    const testItems = getTestItems(5);
    await setupTest(testItems);
    const expectations = testItems.map((item, index) => {
      return {
        name: item.name,
        index: index,
      };
    });
    queryItems().forEach((item, index) => {
      assertEquals(expectations[index]!.name, item.name);
      assertEquals(expectations[index]!.index.toString(), item.id.slice(5));
    });
  });

  test('List size updates', async () => {
    await setupTest(getTestItems(1));
    assertEquals(1, queryItems().length);

    // Ensure that on updating the list with an array smaller in size
    // than the viewport item count, all the array items are rendered.
    const items = getTestItems(3);
    testApp.listItems = items;
    await eventToPromise('viewport-filled', lazyList);
    assertEquals(3, queryItems().length);

    // Ensure that on updating the list with an array greater in size than
    // the viewport item count, only a chunk of array items are rendered.
    testApp.listItems = getTestItems(2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT);
    await eventToPromise('viewport-filled', lazyList);
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems().length);
  });

  test('Scroll', async () => {
    const numItems = 2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    await setupTest(getTestItems(numItems));
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems().length);

    // Scrolling 50% of the viewport renders 50% more items.
    testApp.scrollTop = SAMPLE_AVAIL_HEIGHT / 2;
    await eventToPromise('viewport-filled', testApp);
    await microtasksFinished();

    assertEquals(
        3 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT / 2, queryItems().length);

    // Scrolling to the end renders remaining items.
    testApp.scrollTop = SAMPLE_AVAIL_HEIGHT;
    await eventToPromise('viewport-filled', testApp);
    await microtasksFinished();
    assertEquals(numItems, queryItems().length);

    // Scrolling back to the top --> all items are still rendered.
    testApp.scrollTop = 0;
    await new Promise(resolve => setTimeout(resolve, 1));
    assertEquals(numItems, queryItems().length);
  });

  test('Restores focus', async () => {
    const numItems = SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    await setupTest(getTestItems(numItems));
    const items = queryItems();
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, items.length);
    const button = items[1]!.shadowRoot.querySelector('button');
    assertTrue(!!button);
    button.focus();
    assertEquals(getDeepActiveElement(), button);

    // Change items
    testApp.listItems = getTestItems(numItems + 1).slice(1);
    await eventToPromise('focus-restored-for-test', lazyList);
    const newItems = queryItems();
    const newButton = newItems[0]!.shadowRoot.querySelector('button');
    const active = getDeepActiveElement();
    assertEquals(active, newButton);
  });

  test('Responds to parent size changes', async () => {
    const numItems = 2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    await setupTest(getTestItems(numItems));
    const items = queryItems();
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, items.length);

    // Change parent height.
    testApp.style.maxHeight = `${SAMPLE_AVAIL_HEIGHT / 2}px`;
    testApp.style.height = `${SAMPLE_AVAIL_HEIGHT / 2}px`;
    await microtasksFinished();
    // Items are not removed.
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems().length);

    testApp.style.maxHeight = '0px';
    testApp.style.height = '0px';
    await microtasksFinished();
    // Items are not removed.
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems().length);

    testApp.style.maxHeight = `${SAMPLE_AVAIL_HEIGHT * 2}px`;
    testApp.style.height = `${SAMPLE_AVAIL_HEIGHT * 2}px`;
    await eventToPromise('viewport-filled', lazyList);
    // Items are added for the taller viewport.
    assertEquals(2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems().length);
  });

  test('Scroll with offset', async () => {
    const numItems = 2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    // Set up with a scroll offset equal to 2 items of height.
    await setupTest(getTestItems(numItems), SAMPLE_ITEM_HEIGHT * 2);

    // 2 fewer items are rendered since the scrollOffset fills the first 2
    // items of space.
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT - 2, queryItems().length);

    // Scrolling 50% of the viewport renders half a viewport more items.
    testApp.scrollTop = SAMPLE_AVAIL_HEIGHT / 2;
    await eventToPromise('viewport-filled', testApp);
    assertEquals(
        3 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT / 2 - 2, queryItems().length);

    // Scrolling to the end renders remaining items. Note the end is 2 items
    // of height past SAMPLE_AVAIL_HEIGHT in this case due to the offset.
    testApp.scrollTop = SAMPLE_AVAIL_HEIGHT + 2 * SAMPLE_ITEM_HEIGHT;
    await eventToPromise('viewport-filled', testApp);
    assertEquals(numItems, queryItems().length);

    // Scrolling back to the top --> all items are still rendered.
    testApp.scrollTop = 0;
    await new Promise(resolve => setTimeout(resolve, 1));
    assertEquals(numItems, queryItems().length);
  });

  test('Default scroll target', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testDocumentTargetApp =
        document.createElement('test-document-target-app') as
        TestDocumentTargetApp;
    testDocumentTargetApp.style.display = 'block';
    testDocumentTargetApp.style.overflowY = 'auto';
    testDocumentTargetApp.style.overflowX = 'hidden';
    document.body.appendChild(testDocumentTargetApp);
    testDocumentTargetApp.listItems = getTestItems(3);

    lazyList = testDocumentTargetApp.shadowRoot.querySelector('cr-lazy-list')!;
    assertTrue(!!lazyList);
    await eventToPromise('viewport-filled', lazyList);
    await microtasksFinished();
    assertEquals(3, queryItems().length);

    // Put the app in an overflow state and verify that not all items are
    // rendered.
    const itemsInView = Math.ceil(window.innerHeight / SAMPLE_ITEM_HEIGHT);
    const itemsInList = itemsInView * 2;
    testDocumentTargetApp.listItems = getTestItems(itemsInList);
    await eventToPromise('viewport-filled', lazyList);
    await microtasksFinished();
    assertTrue(itemsInView <= queryItems().length);
    assertTrue(itemsInList > queryItems().length);

    // Scroll the window and ensure items are now rendered.
    window.scrollTo(0, itemsInList * SAMPLE_ITEM_HEIGHT - window.innerHeight);
    await eventToPromise('viewport-filled', lazyList);
    await microtasksFinished();
    assertEquals(itemsInList, queryItems().length);
  });

  test('Throws an error if array length changes in place', async () => {
    await setupTest(getTestItems(3));
    assertEquals(3, queryItems().length);
    let error = '';

    try {
      // Modify lazyList directly to make the error catchable.
      lazyList.items.push(...getTestItems(3));
      lazyList.requestUpdate();
      await lazyList.updateComplete;
    } catch (e) {
      error = (e as Error).message;
    }

    assertEquals(
        'Assertion failed: Items array changed in place; ' +
            'rendered result may be incorrect.',
        error);
  });

  test('List size updates in chunking mode', async () => {
    // Test with a chunkSize of 4 to ensure partially filled chunks
    // work as expected.
    await setupTest(getTestItems(1), /* scrollOffset = */ 0, 4);
    assertEquals(1, queryItems().length);
    // 1 chunk holding the item.
    let chunks = lazyList.querySelectorAll('.chunk');
    assertEquals(1, chunks.length);
    assertEquals(1, chunks[0]!.querySelectorAll('test-item').length);

    // Ensure that on updating the list with an array smaller in size
    // than the viewport item count, all the array items are rendered.
    const items = getTestItems(3);
    testApp.listItems = items;
    await eventToPromise('viewport-filled', lazyList);
    assertEquals(3, queryItems().length);
    // Still 1 chunk holding the items.
    chunks = lazyList.querySelectorAll('.chunk');
    assertEquals(1, chunks.length);
    assertEquals(3, chunks[0]!.querySelectorAll('test-item').length);

    // Ensure that on updating the list with an array greater in size than
    // the viewport item count, only a chunk of array items are rendered.
    testApp.listItems = getTestItems(2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT);
    await eventToPromise('viewport-filled', lazyList);
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems().length);
    // 2 chunks holding the items.
    chunks = lazyList.querySelectorAll('.chunk');
    assertEquals(2, chunks.length);
    assertEquals(4, chunks[0]!.querySelectorAll('test-item').length);
    assertEquals(
        SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT - 4,
        chunks[1]!.querySelectorAll('test-item').length);
  });

  test('Scroll in chunking mode', async () => {
    const numItems = 2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    await setupTest(getTestItems(numItems), /* scrollOffset = */ 0, 4);
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems().length);

    // 2 chunks holding the items.
    let chunks = lazyList.querySelectorAll('.chunk');
    assertEquals(2, chunks.length);
    assertEquals(4, chunks[0]!.querySelectorAll('test-item').length);
    assertEquals(2, chunks[1]!.querySelectorAll('test-item').length);

    // Scrolling 50% of the viewport renders 50% more items.
    let listFilled = eventToPromise('viewport-filled', testApp);
    testApp.scrollTop = SAMPLE_AVAIL_HEIGHT / 2;
    await listFilled;
    await microtasksFinished();

    assertEquals(
        3 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT / 2, queryItems().length);
    // 3 chunks holding the items (9 items = 4 + 4 + 1)
    chunks = lazyList.querySelectorAll('.chunk');
    assertEquals(3, chunks.length);
    assertEquals(4, chunks[0]!.querySelectorAll('test-item').length);
    assertEquals(4, chunks[1]!.querySelectorAll('test-item').length);
    assertEquals(1, chunks[2]!.querySelectorAll('test-item').length);

    // Scrolling to the end renders remaining items.
    listFilled = eventToPromise('viewport-filled', testApp);
    testApp.scrollTop = SAMPLE_AVAIL_HEIGHT;
    await listFilled;
    await microtasksFinished();
    assertEquals(numItems, queryItems().length);
    // 3 chunks holding the items, now all are full.
    chunks = lazyList.querySelectorAll('.chunk');
    assertEquals(3, chunks.length);
    for (const chunk of chunks) {
      assertEquals(4, chunk.querySelectorAll('test-item').length);
    }

    // Scrolling back to the top --> all items are still rendered.
    testApp.scrollTop = 0;
    await new Promise(resolve => setTimeout(resolve, 1));
    assertEquals(numItems, queryItems().length);
    // Still 3 chunks holding the items.
    chunks = lazyList.querySelectorAll('.chunk');
    assertEquals(3, chunks.length);
    for (const chunk of chunks) {
      assertEquals(4, chunk.querySelectorAll('test-item').length);
    }
  });

  test('Restores focus in chunking mode', async () => {
    const numItems = SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    await setupTest(getTestItems(numItems), /* scrollOffset= */ 0, 4);
    const items = queryItems();
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, items.length);
    const button = items[1]!.shadowRoot.querySelector('button');
    assertTrue(!!button);
    button.focus();
    assertEquals(getDeepActiveElement(), button);

    // Change items
    testApp.listItems = getTestItems(numItems + 1).slice(1);
    await eventToPromise('focus-restored-for-test', lazyList);
    const newItems = queryItems();
    const newButton = newItems[0]!.shadowRoot.querySelector('button');
    const active = getDeepActiveElement();
    assertEquals(active, newButton);
  });

  function setUpListPaddingApp(chunkSize: number = 0): TestListPaddingApp {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testListPaddingApp =
        document.createElement('test-list-padding-app') as TestListPaddingApp;
    testListPaddingApp.style.display = 'block';
    testListPaddingApp.style.overflowY = 'auto';
    testListPaddingApp.style.overflowX = 'hidden';
    testListPaddingApp.style.height = `${SAMPLE_AVAIL_HEIGHT}px`;
    testListPaddingApp.style.maxHeight = `${SAMPLE_AVAIL_HEIGHT}px`;
    testListPaddingApp.chunkSize = chunkSize;
    document.body.appendChild(testListPaddingApp);
    return testListPaddingApp;
  }

  test('List padding does not change item estimates', async () => {
    const testListPaddingApp = setUpListPaddingApp();
    testListPaddingApp.listItems = getTestItems(12);

    lazyList = testListPaddingApp.shadowRoot.querySelector('cr-lazy-list')!;
    assertTrue(!!lazyList);
    await eventToPromise('viewport-filled', lazyList);
    await microtasksFinished();
    // Should render 6 items, because exactly 6 fit in the viewport.
    assertEquals(6, queryItems().length);
  });

  test('List padding does not change item estimates', async () => {
    const testListPaddingApp = setUpListPaddingApp(3);
    testListPaddingApp.listItems = getTestItems(12);

    lazyList = testListPaddingApp.shadowRoot.querySelector('cr-lazy-list')!;
    assertTrue(!!lazyList);
    await eventToPromise('viewport-filled', lazyList);
    await microtasksFinished();
    // Should render 6 items, because exactly 6 fit in the viewport.
    assertEquals(6, queryItems().length);
  });

  test('Fires items-rendered event', async () => {
    await setupTest(getTestItems(1));
    assertEquals(1, queryItems().length);

    const items = getTestItems(12);
    // Fires event when the list adds items.
    testApp.listItems = items.slice(0, 6);
    await eventToPromise('items-rendered', lazyList);
    assertEquals(6, queryItems().length);

    // Still fires the event if the list changes to a list with the same
    // length and different items.
    testApp.listItems = items.slice(6);
    await eventToPromise('items-rendered', lazyList);
    assertEquals(6, queryItems().length);

    // Event fires if list changes to shorter length (e.g. items removed).
    testApp.listItems = items.slice(6, 8);
    await eventToPromise('items-rendered', lazyList);
    assertEquals(2, queryItems().length);

    testApp.listItems = [];
    await eventToPromise('items-rendered', lazyList);
    assertEquals(0, queryItems().length);
  });

  class TestListWithVariedHeightsApp extends CrLitElement {
    static get is() {
      return 'test-list-with-varied-heights-app';
    }

    static override get properties() {
      return {
        itemSize: {type: Number},
        listItems: {type: Array},
      };
    }

    accessor itemSize: number|undefined;
    accessor listItems: Array<{height: number}> = [];

    static override get styles() {
      return css`
        :host {
          display: block;
          overflow: auto;
        }
      `;
    }

    override render() {
      return html`
      <cr-lazy-list
          .items="${this.listItems}" .itemSize="${this.itemSize}"
          .chunkSize="${10}"
          .scrollTarget="${this}"
          .template=${(item: {height: number}) => html`
              <div class="item" style="height: ${item.height}px"></div>
            `}>
      </cr-lazy-list>`;
    }
  }

  customElements.define(
      TestListWithVariedHeightsApp.is, TestListWithVariedHeightsApp);

  test('Uses itemSize property instead of calculating', async () => {
    const viewSize = 100;
    const typicalItemSize = 20;
    // Add a couple of tall items and then a bunch of typically sized items.
    const items = [
      {height: 50},
      {height: 50},
      ...[...Array(20)].map(() => ({height: typicalItemSize})),
    ];

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    let testApp = document.createElement('test-list-with-varied-heights-app') as
        TestListWithVariedHeightsApp;
    testApp.listItems = items;
    testApp.itemSize = typicalItemSize;
    testApp.style.height = `${viewSize}px`;
    document.body.appendChild(testApp);

    let lazyList = testApp.shadowRoot.querySelector('cr-lazy-list');
    assertTrue(!!lazyList);
    await eventToPromise('viewport-filled', lazyList);
    assertEquals(
        viewSize / typicalItemSize, lazyList.querySelectorAll('.item').length,
        'Number of items created should depend the typical item size');

    // No itemSize specified should mean the actual height is used.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testApp = document.createElement('test-list-with-varied-heights-app') as
        TestListWithVariedHeightsApp;
    testApp.listItems = items;
    testApp.style.height = `${viewSize}px`;
    document.body.appendChild(testApp);
    lazyList = testApp.shadowRoot.querySelector('cr-lazy-list');
    assertTrue(!!lazyList);
    await eventToPromise('viewport-filled', lazyList);
    assertEquals(
        viewSize / items[0]!.height, lazyList.querySelectorAll('.item').length,
        'Number of items created should reflect the height of the first item.');
  });
});
