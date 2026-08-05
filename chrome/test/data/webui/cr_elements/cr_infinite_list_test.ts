// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';

import type {CrInfiniteListElement} from 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import {FocusRowMixinLit} from 'chrome://resources/cr_elements/focus_row_mixin_lit.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

const SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT = 6;
const SAMPLE_ITEM_HEIGHT = 56;
const SAMPLE_AVAIL_HEIGHT =
    SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT * SAMPLE_ITEM_HEIGHT;

class TestItemElement extends CrLitElement {
  static get is() {
    return 'test-item';
  }

  override render() {
    return html`
<div style="height: 48px; padding: 4px;">
  <span>${this.name}</span>
  <button>click item</button>
</div>`;
  }

  static override get properties() {
    return {
      name: {type: String},
    };
  }

  override focus() {
    const button = this.shadowRoot.querySelector('button');
    assertTrue(!!button);
    button.focus();
  }

  accessor name: string = '';
}

customElements.define(TestItemElement.is, TestItemElement);

// Test elements for validating interaction between cr-infinite-list
// and clients using FocusRowMixinLit.
const TestFocusRowItemElementBase = FocusRowMixinLit(CrLitElement);

class TestFocusRowItemElement extends TestFocusRowItemElementBase {
  static get is() {
    return 'test-focus-row-item';
  }

  override render() {
    return html`
<div focus-row-container style="height: 48px; padding: 4px;">
  <span>${this.name}</span>
  <button id="btn1" focus-row-control focus-type="btn1">Button 1</button>
  <button id="btn2" focus-row-control focus-type="btn2">Button 2</button>
</div>`;
  }

  static override get properties() {
    return {
      ...super.properties,
      name: {type: String},
    };
  }

  accessor name: string = '';
}

customElements.define(TestFocusRowItemElement.is, TestFocusRowItemElement);

class TestFocusRowAppElement extends CrLitElement {
  static get is() {
    return 'test-focus-row-app';
  }

  override render() {
    // clang-format off
    return html`
      <cr-infinite-list .items="${this.listItems}" style="flex: 1;"
          .itemSize="${SAMPLE_ITEM_HEIGHT}"
          @restore-list-focus="${this.onRestoreListFocus}"
          .template=${
            (item: {name: string}, idx: number, tabidx: number) =>
                html`<test-focus-row-item name="${item.name}" id="item-${idx}"
                       tabindex="${tabidx}"
                       .listTabIndex="${tabidx}"
                       .focusRowIndex="${idx}"
                       .lastFocused="${this.lastFocused}"
                       @last-focused-changed="${this.onLastFocusedChanged}"
                       .listBlurred="${this.listBlurred}"
                       @list-blurred-changed="${this.onListBlurredChanged}">
                   </test-focus-row-item>`}>
      </cr-infinite-list>`;
    // clang-format on
  }

  static override get properties() {
    return {
      listItems: {type: Array},
      lastFocused: {type: Object},
      listBlurred: {type: Boolean},
    };
  }

  accessor listItems: Array<{name: string}> = [];
  accessor lastFocused: HTMLElement|null = null;
  accessor listBlurred: boolean = true;

  protected onLastFocusedChanged(e: CustomEvent<{value: HTMLElement}>) {
    this.lastFocused = e.detail.value;
  }

  protected onListBlurredChanged(e: CustomEvent<{value: boolean}>) {
    this.listBlurred = e.detail.value;
  }

  protected onRestoreListFocus() {
    this.listBlurred = false;
  }
}

customElements.define(TestFocusRowAppElement.is, TestFocusRowAppElement);

class TestAppElement extends CrLitElement {
  static get is() {
    return 'test-app';
  }

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

  static override get properties() {
    return {
      listItems: {type: Array},
      useDefaultScroll: {type: Boolean},
    };
  }

  accessor listItems: Array<{name: string}> = [];
  accessor useDefaultScroll: boolean = false;
}

customElements.define(TestAppElement.is, TestAppElement);

function queryItems(infiniteList: CrInfiniteListElement<{name: string}>):
    NodeListOf<TestItemElement> {
  return infiniteList.querySelectorAll<TestItemElement>('test-item');
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

function getKeyboardFocusableItem(
    infiniteList: CrInfiniteListElement<{name: string}>): TestItemElement {
  const item =
      infiniteList.querySelector<TestItemElement>('test-item[tabindex="0"]');
  assertTrue(!!item);
  return item;
}

function createTestApp(useDefaultScroll: boolean = false): TestAppElement {
  const testApp = document.createElement('test-app') as TestAppElement;
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
  let infiniteList: CrInfiniteListElement<{name: string}>;
  let testApp: TestAppElement;
  let innerList: HTMLElement;

  async function setupTest(
      sampleData: Array<{name: string}>, useDefaultScroll: boolean = false) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testApp = createTestApp(useDefaultScroll);
    testApp.listItems = sampleData;

    infiniteList =
        testApp.shadowRoot.querySelector<CrInfiniteListElement<{name: string}>>(
            'cr-infinite-list')!;
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
  let infiniteList: CrInfiniteListElement<{name: string}>;
  let testApp: TestAppElement;
  let innerList: HTMLElement;

  async function setupTest(sampleData: Array<{name: string}>) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testApp = createTestApp();
    testApp.listItems = sampleData;

    infiniteList =
        testApp.shadowRoot.querySelector<CrInfiniteListElement<{name: string}>>(
            'cr-infinite-list')!;
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
    const button = renderedItems[2]!.shadowRoot.querySelector('button');
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

  test('Restores focus when items change', async () => {
    const numItems = SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    const testItems = getTestItems(numItems);
    await setupTest(testItems);
    const renderedItems = queryItems(infiniteList);
    assertEquals(SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT, renderedItems.length);

    // Focus third item ('Three')
    const button = renderedItems[2]!.shadowRoot.querySelector('button');
    assertTrue(!!button);
    button.focus();
    assertEquals(getDeepActiveElement(), button);

    // Update list items
    const newItems = getTestItems(numItems + 1).slice(1);
    testApp.listItems = newItems;
    await eventToPromise('viewport-filled', infiniteList);
    await microtasksFinished();

    // Focus should be restored to the item at index 2 ('Four' in new array)
    const updatedItems = queryItems(infiniteList);
    const newButton = updatedItems[2]!.shadowRoot.querySelector('button');
    assertEquals(getDeepActiveElement(), newButton);
  });

  test('Restores focus when focused item is removed repeatedly', async () => {
    const numItems = SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    const testItems = getTestItems(numItems);
    await setupTest(testItems);
    let renderedItems = queryItems(infiniteList);

    let restoreFocusEventCount = 0;
    infiniteList.addEventListener('restore-list-focus', () => {
      restoreFocusEventCount++;
    });

    // Focus last rendered item ('Six' at index 5)
    let button = renderedItems[5]!.shadowRoot.querySelector('button');
    assertTrue(!!button);
    button.focus();
    assertEquals(getDeepActiveElement(), button);

    // 1st removal: Remove last item so length decreases from 6 to 5
    testApp.listItems = getTestItems(numItems - 1);
    await eventToPromise('viewport-filled', infiniteList);
    await microtasksFinished();

    // Focus should be restored to item at clamped index 4 ('Five')
    renderedItems = queryItems(infiniteList);
    button = renderedItems[4]!.shadowRoot.querySelector('button');
    assertEquals(getDeepActiveElement(), button);
    assertEquals(1, restoreFocusEventCount);

    // 2nd removal: Remove last item so length decreases from 5 to 4
    testApp.listItems = getTestItems(numItems - 2);
    await eventToPromise('viewport-filled', infiniteList);
    await microtasksFinished();

    // Focus should be restored to item at clamped index 3 ('Four')
    renderedItems = queryItems(infiniteList);
    button = renderedItems[3]!.shadowRoot.querySelector('button');
    assertEquals(getDeepActiveElement(), button);
    assertEquals(2, restoreFocusEventCount);

    // 3rd removal: Remove last item so length decreases from 4 to 3
    testApp.listItems = getTestItems(numItems - 3);
    await eventToPromise('viewport-filled', infiniteList);
    await microtasksFinished();

    // Focus should be restored to item at clamped index 2 ('Three')
    renderedItems = queryItems(infiniteList);
    button = renderedItems[2]!.shadowRoot.querySelector('button');
    assertEquals(getDeepActiveElement(), button);
    assertEquals(3, restoreFocusEventCount);
  });

  test('Does not restore focus if list was not focused', async () => {
    const numItems = SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    const testItems = getTestItems(numItems);
    await setupTest(testItems);

    let restoreFocusEventCount = 0;
    infiniteList.addEventListener('restore-list-focus', () => {
      restoreFocusEventCount++;
    });

    // Ensure focus is outside infiniteList
    document.body.focus();
    const activeBefore = getDeepActiveElement();

    // Update list items
    testApp.listItems = getTestItems(numItems + 1);
    await eventToPromise('viewport-filled', infiniteList);
    await microtasksFinished();

    // Focus should remain unchanged, no restore-list-focus event.
    assertEquals(0, restoreFocusEventCount);
    assertEquals(getDeepActiveElement(), activeBefore);
  });

  test('Restores focus after moving focus between items', async () => {
    const numItems = SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    const testItems = getTestItems(numItems);
    await setupTest(testItems);
    const renderedItems = queryItems(infiniteList);

    // Focus first item ('One')
    const button0 = renderedItems[0]!.shadowRoot.querySelector('button');
    assertTrue(!!button0);
    button0.focus();
    assertEquals(getDeepActiveElement(), button0);

    // Move focus to third item ('Three')
    const button2 = renderedItems[2]!.shadowRoot.querySelector('button');
    assertTrue(!!button2);
    button2.focus();
    assertEquals(getDeepActiveElement(), button2);

    // Update list items
    const newItems = getTestItems(numItems + 1).slice(1);
    testApp.listItems = newItems;
    await eventToPromise('viewport-filled', infiniteList);
    await microtasksFinished();

    // Focus should be restored to the item at index 2 ('Four' in new array)
    const updatedItems = queryItems(infiniteList);
    const newButton = updatedItems[2]!.shadowRoot.querySelector('button');
    assertEquals(getDeepActiveElement(), newButton);
  });

  // Tests interaction between cr-infinite-list and FocusRowMixinLit clients
  // works as expected with arrow key navigation.
  test('FocusMixinLit integration', async () => {
    const numItems = SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT;
    const testItems = getTestItems(numItems);

    const testApp =
        document.createElement('test-focus-row-app') as TestFocusRowAppElement;
    testApp.style.display = 'flex';
    testApp.style.height = `${SAMPLE_AVAIL_HEIGHT}px`;
    testApp.listItems = testItems;
    document.body.appendChild(testApp);

    const infiniteList =
        testApp.shadowRoot.querySelector<CrInfiniteListElement<{name: string}>>(
            'cr-infinite-list')!;
    await eventToPromise('viewport-filled', infiniteList);
    await microtasksFinished();

    let renderedItems = infiniteList.querySelectorAll<TestFocusRowItemElement>(
        'test-focus-row-item');

    assertEquals(6, renderedItems.length);
    renderedItems[0]!.focus();
    const firstButtonItem0 =
        renderedItems[0]!.shadowRoot.querySelector<HTMLElement>('#btn1');
    assertTrue(!!firstButtonItem0);
    assertEquals(getDeepActiveElement(), firstButtonItem0);

    // Simulate right arrow event on the button, which moves focus to button
    // 2.
    firstButtonItem0.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowRight', bubbles: true, composed: true}));
    const secondButtonItem0 =
        renderedItems[0]!.shadowRoot.querySelector<HTMLElement>('#btn2');
    assertTrue(!!secondButtonItem0);
    await microtasksFinished();
    assertEquals(getDeepActiveElement(), secondButtonItem0);

    // Press ArrowDown to navigate to second item
    secondButtonItem0.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await microtasksFinished();

    // Equivalent element (#btn2) in second item should be focused
    const secondButtonItem1 =
        renderedItems[1]!.shadowRoot.querySelector<HTMLElement>('#btn2');
    assertTrue(!!secondButtonItem1);
    assertEquals(getDeepActiveElement(), secondButtonItem1);

    // Left arrow goes back to the first item.
    const firstButtonItem1 =
        renderedItems[1]!.shadowRoot.querySelector<HTMLElement>('#btn1');
    assertTrue(!!firstButtonItem1);
    secondButtonItem1.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowLeft', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(firstButtonItem1, getDeepActiveElement());

    // Up arrow goes back to first button, first row.
    firstButtonItem1.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowUp', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(firstButtonItem0, getDeepActiveElement());

    // Focus the last item.
    const lastButton =
        renderedItems[5]!.shadowRoot.querySelector<HTMLElement>('#btn2');
    assertTrue(!!lastButton);
    lastButton.focus();
    assertEquals(getDeepActiveElement(), lastButton);

    // Remove last item so length decreases from 6 to 5
    testApp.listItems = getTestItems(numItems - 1);
    await eventToPromise('viewport-filled', infiniteList);
    await microtasksFinished();

    // Focus should be restored to item at clamped index 4 ('Five')
    renderedItems = infiniteList.querySelectorAll<TestFocusRowItemElement>(
        'test-focus-row-item');
    const newLastButton =
        renderedItems[4]!.shadowRoot.querySelector<HTMLElement>('#btn2');
    assertTrue(!!newLastButton);
    assertEquals(getDeepActiveElement(), newLastButton);

    // Arrow down again keeps focus on the same element.
    newLastButton.dispatchEvent(new KeyboardEvent(
        'keydown', {key: 'ArrowDown', bubbles: true, composed: true}));
    await microtasksFinished();
    assertEquals(getDeepActiveElement(), newLastButton);
  });
});
