// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';

import type {CrLazyListElement} from 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
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
    this.shadowRoot!.querySelector('button')!.focus();
  }

  name: string = '';
}

customElements.define('test-item', TestItem);

class TestApp extends CrLitElement {
  static get is() {
    return 'test-app';
  }

  static override get properties() {
    return {
      listItems: {type: Array},
      restoreFocusElement_: {type: Object},
      scrollOffset: {type: Number},
    };
  }

  listItems: Array<{name: string}> = [];
  scrollOffset: number = 0;
  private restoreFocusElement_: HTMLElement|null = null;

  override render() {
    return html`
    <div style="height: ${this.scrollOffset}px"></div>
    <cr-lazy-list .items="${this.listItems}" .scrollTarget="${this}"
        .scrollOffset="${this.scrollOffset}"
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
    this.restoreFocusElement_ = this.shadowRoot!.querySelector('[name="Two"]');
  }
}

customElements.define('test-app', TestApp);

suite('CrLazyListTest', () => {
  let lazyList: CrLazyListElement;
  let testApp: TestApp;

  async function setupTest(
      sampleData: Array<{name: string}>, scrollOffset: number = 0) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testApp = document.createElement('test-app') as TestApp;
    testApp.style.height = `${SAMPLE_AVAIL_HEIGHT}px`;
    testApp.style.maxHeight = `${SAMPLE_AVAIL_HEIGHT}px`;
    testApp.style.display = 'block';
    testApp.style.overflowY = 'auto';
    testApp.style.overflowX = 'hidden';
    document.body.appendChild(testApp);
    testApp.listItems = sampleData;
    testApp.scrollOffset = scrollOffset;

    lazyList = testApp.shadowRoot!.querySelector('cr-lazy-list')!;
    assertTrue(!!lazyList);
    await eventToPromise('viewport-filled', lazyList);
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
    return items.slice(0, count);
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
    await eventToPromise('fill-height-end', testApp);

    assertEquals(
        3 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT / 2, queryItems().length);

    // Scrolling to the end renders remaining items.
    testApp.scrollTop = SAMPLE_AVAIL_HEIGHT;
    await eventToPromise('fill-height-end', testApp);
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
    const button = items[1]!.shadowRoot!.querySelector('button');
    assertTrue(!!button);
    button.focus();
    assertEquals(getDeepActiveElement(), button);

    // Change items
    testApp.listItems = getTestItems(numItems + 1).slice(1);
    await eventToPromise('focus-restored-for-test', lazyList);
    const newItems = queryItems();
    const newButton = newItems[0]!.shadowRoot!.querySelector('button');
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
});
