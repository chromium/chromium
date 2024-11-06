// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';

import type {CrInfiniteListElement} from 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
      name: {type: String},
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
    const button = this.shadowRoot!.querySelector('button');
    assertTrue(!!button);
    button.focus();
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
      useDefaultScroll: {type: Boolean},
    };
  }

  listItems: Array<{name: string}> = [];
  useDefaultScroll: boolean = false;

  override render() {
    return this.useDefaultScroll ?
        html`
      <cr-infinite-list .items="${this.listItems}" style="flex: 1;"
          .itemSize="${SAMPLE_ITEM_HEIGHT}"
          .template=${
            (item: {name: string}, idx: number, tabidx: number) =>
                html`<test-item name="${item.name}" id="item-${idx}"
                       tabindex="${tabidx}">
                   </test-item>`}>
      </cr-infinite-list>` :
        html`
      <cr-infinite-list .items="${this.listItems}" .scrollTarget="${this}"
          .itemSize="${SAMPLE_ITEM_HEIGHT}"
          .template=${
            (item: {name: string}, idx: number, tabidx: number) =>
                html`<test-item name="${item.name}" id="item-${idx}"
                       tabindex="${tabidx}">
                   </test-item>`}>
      </cr-infinite-list>`;
  }
}

customElements.define('test-app', TestApp);

function queryItems(infiniteList: CrInfiniteListElement): NodeListOf<TestItem> {
  return infiniteList.querySelectorAll<TestItem>('test-item');
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

function getKeyboardFocusableItem(infiniteList: CrInfiniteListElement):
    TestItem {
  const item = infiniteList.querySelector<TestItem>('test-item[tabindex="0"]');
  assertTrue(!!item);
  return item;
}

function createTestApp(useDefaultScroll: boolean = false): TestApp {
  const testApp = document.createElement('test-app') as TestApp;
  testApp.useDefaultScroll = useDefaultScroll;
  testApp.style.height = `${SAMPLE_AVAIL_HEIGHT}px`;
  testApp.style.maxHeight = `${SAMPLE_AVAIL_HEIGHT}px`;
  testApp.style.display = useDefaultScroll ? 'flex' : 'block';
  if (useDefaultScroll) {
    testApp.style.flexDirection = 'column';
  }
  testApp.style.overflowY = useDefaultScroll ? 'hidden' : 'auto';
  testApp.style.overflowX = 'hidden';
  document.body.appendChild(testApp);
  return testApp;
}

suite('InfiniteListTest', () => {
  let infiniteList: CrInfiniteListElement;
  let testApp: TestApp;
  let innerList: HTMLElement;

  async function setupTest(
      sampleData: Array<{name: string}>, useDefaultScroll: boolean = false) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testApp = createTestApp(useDefaultScroll);
    testApp.listItems = sampleData;

    infiniteList = testApp.shadowRoot!.querySelector('cr-infinite-list')!;
    const lazyList = infiniteList.querySelector('cr-lazy-list');
    assertTrue(!!lazyList);
    innerList = lazyList;
    await eventToPromise('viewport-filled', infiniteList);
  }

  test('Populates template and size parameters correctly', async () => {
    const testItems = getTestItems(5);
    await setupTest(testItems);
    const expectations = testItems.map((item, index) => {
      return {
        name: item.name,
        index: index,
        tabindex: index === 0 ? 0 : -1,
      };
    });
    queryItems(infiniteList).forEach((item, index) => {
      assertEquals(expectations[index]!.name, item.name);
      assertEquals(expectations[index]!.index.toString(), item.id.slice(5));
      assertEquals(expectations[index]!.tabindex, item.tabIndex);
      assertEquals(
          'auto 56px',
          (item.computedStyleMap().get('contain-intrinsic-size') as
           CSSStyleValue)
              .toString());
    });
  });

  test('Arrow key navigation', async () => {
    const numItems = 2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    const testItems = getTestItems(numItems);
    await setupTest(testItems);
    assertEquals(
        SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems(infiniteList).length);
    let focusable = getKeyboardFocusableItem(infiniteList);
    assertEquals('One', focusable.name);

    innerList.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    await microtasksFinished();
    focusable = getKeyboardFocusableItem(infiniteList);
    assertEquals('Two', focusable.name);

    innerList.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    await microtasksFinished();
    focusable = getKeyboardFocusableItem(infiniteList);
    assertEquals('One', focusable.name);

    // Proceed to the last rendered item.
    for (let i = 1; i < SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT; i++) {
      innerList.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
      await microtasksFinished();
      focusable = getKeyboardFocusableItem(infiniteList);
      assertEquals(testItems[i]!.name, focusable.name);
    }

    // Confirm that keydown from the last rendered item renders the next item
    // and makes it the focusable item.
    assertEquals(
        SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems(infiniteList).length);
    innerList.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    await eventToPromise('viewport-filled', infiniteList);
    // The exact number of items rendered will depend on where the browser
    // scrolls to when scrollIntoViewIfNeeded() is called, but it should always
    // be greater than the number of viewport items so that the correct
    // focusable item is rendered.
    assertLT(
        SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems(infiniteList).length);
    focusable = getKeyboardFocusableItem(infiniteList);
    assertEquals('Seven', focusable.name);
  });


  test('Default scroll target', async () => {
    const numItems = 2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    const testItems = getTestItems(numItems);
    await setupTest(testItems, true);
    assertEquals(
        SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, queryItems(infiniteList).length);
    // Overflow-y is set to auto, since the infinite list itself is the
    // scrolling container.
    assertEquals(
        'auto',
        (infiniteList.computedStyleMap().get('overflow-y') as CSSKeywordValue)
            .value);

    // Scrolling the list element renders all items.
    infiniteList.scrollTop = SAMPLE_AVAIL_HEIGHT;
    await eventToPromise('viewport-filled', infiniteList);
    assertEquals(numItems, queryItems(infiniteList).length);
  });
});

suite('InfiniteListFocusTest', () => {
  let infiniteList: CrInfiniteListElement;
  let testApp: TestApp;
  let innerList: HTMLElement;

  async function setupTest(sampleData: Array<{name: string}>) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testApp = createTestApp();
    testApp.listItems = sampleData;

    infiniteList = testApp.shadowRoot!.querySelector('cr-infinite-list')!;
    const lazyList = infiniteList.querySelector('cr-lazy-list');
    assertTrue(!!lazyList);
    innerList = lazyList;
    await eventToPromise('viewport-filled', infiniteList);
  }

  test('Focus change', async () => {
    const numItems = 2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    const testItems = getTestItems(numItems);
    await setupTest(testItems);
    const renderedItems = queryItems(infiniteList);
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, renderedItems.length);
    let focusable = getKeyboardFocusableItem(infiniteList);
    assertEquals('One', focusable.name);

    // Focus and click on the 3rd item in the list's button.
    const button = renderedItems[2]!.shadowRoot!.querySelector('button');
    assertTrue(!!button);
    button.focus();
    button.click();

    await microtasksFinished();
    focusable = getKeyboardFocusableItem(infiniteList);
    assertEquals('Three', focusable.name);

    // Key events navigate from the newly focusable item.
    innerList.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    await microtasksFinished();
    focusable = getKeyboardFocusableItem(infiniteList);
    assertEquals('Four', focusable.name);
  });
});
