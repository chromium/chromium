// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the RepeatDirective class. */
import 'chrome://extensions/extensions.js';

import {PromiseResolver} from '//resources/js/promise_resolver.js';
import {asyncMap} from 'chrome://extensions/extensions.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertGT, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

suite('AsyncMapDirectiveTest', function() {
  let initialCount: number = 3;
  let testElement: TestElement;
  class TestElement extends CrLitElement {
    static get is() {
      return 'test-element';
    }

    override render() {
      return html`
         <div @rendered-items-changed="${this.onRenderedItemsChanged_}">
           ${
          asyncMap(
              this.items, item => html`<div class="item">${item}</div>`,
              initialCount, this.filter)}
         </div>
       `;
    }

    static override get properties() {
      return {
        items: {type: Array},
        filter: {type: Object},
      };
    }

    items: string[] = [
      'One',
      'Two',
      'Three',
      'Four',
      'Five',
      'Six',
      'Seven',
      'Eight',
      'Nine',
      'Ten',
      'Eleven',
      'Twelve',
    ];
    filter: ((item: string) => boolean)|null = null;
    private itemsRendered_: number[] = [];
    private allItemsRendered_: PromiseResolver<number[]> =
        new PromiseResolver<number[]>();

    private onRenderedItemsChanged_(e: CustomEvent<number>) {
      this.itemsRendered_.push(e.detail);
      const matchingItems = this.filter === null ?
          this.items :
          this.items.filter(item => this.filter!(item));
      if (e.detail === matchingItems.length) {
        this.allItemsRendered_.resolve(this.itemsRendered_);
      }
    }

    allItemsRendered(): Promise<number[]> {
      return this.allItemsRendered_.promise;
    }

    reset() {
      this.allItemsRendered_ = new PromiseResolver<number[]>();
      this.itemsRendered_ = [];
    }

    splice(start: number, deleteCount: number, insertions: string[] = []) {
      this.items.splice(start, deleteCount, ...insertions);
      this.requestUpdate();
    }
  }

  customElements.define(TestElement.is, TestElement);

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('Basic', async () => {
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);

    let itemsRendered = await testElement.allItemsRendered();
    // Additive increase means that we render 3 items, then at most 6 on the
    // second cycle. So we should have at least 3 calls.
    assertGT(itemsRendered.length, 2);
    assertEquals(3, itemsRendered[0]);
    assertEquals(12, itemsRendered[itemsRendered.length - 1]);

    // Make sure the items are actually in the DOM.
    assertEquals(12, testElement.shadowRoot!.querySelectorAll('.item').length);

    // Test applying a filter.
    testElement.reset();
    testElement.filter = (item: string) => item.startsWith('T');
    itemsRendered = await testElement.allItemsRendered();
    assertEquals(4, itemsRendered[itemsRendered.length - 1]);
    let items = testElement.shadowRoot!.querySelectorAll('.item');
    assertEquals(4, items.length);
    assertEquals('Two', items[0]!.textContent!);
    assertEquals('Three', items[1]!.textContent!);
    assertEquals('Ten', items[2]!.textContent!);
    assertEquals('Twelve', items[3]!.textContent!);

    // Filter with no matches
    testElement.reset();
    testElement.filter = (item: string) => item.startsWith('Z');
    itemsRendered = await testElement.allItemsRendered();
    assertEquals(1, itemsRendered.length);
    assertEquals(0, itemsRendered[0]);
    assertEquals(0, testElement.shadowRoot!.querySelectorAll('.item').length);

    // Clear the filter.
    testElement.reset();
    testElement.filter = null;
    itemsRendered = await testElement.allItemsRendered();
    assertEquals(12, itemsRendered[itemsRendered.length - 1]);
    items = testElement.shadowRoot!.querySelectorAll('.item');
    assertEquals(12, items.length);
  });

  test('Different initial count', async () => {
    initialCount = 6;
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);

    const itemsRendered = await testElement.allItemsRendered();
    assertNotEquals(0, itemsRendered.length);
    assertEquals(6, itemsRendered[0]);
    assertEquals(12, itemsRendered[itemsRendered.length - 1]);

    // Make sure the items are actually in the DOM.
    assertEquals(12, testElement.shadowRoot!.querySelectorAll('.item').length);
  });

  test('Modify list', async () => {
    // Verifies the list updates correctly when the test element updates the
    // list.
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);

    let itemsRendered = await testElement.allItemsRendered();
    assertEquals(12, itemsRendered[itemsRendered.length - 1]);
    assertEquals(12, testElement.shadowRoot!.querySelectorAll('.item').length);

    // Set a new array.
    testElement.reset();
    testElement.items = ['Hello', 'World', 'Goodbye'];
    itemsRendered = await testElement.allItemsRendered();
    assertEquals(3, itemsRendered[itemsRendered.length - 1]);
    const renderedItems = testElement.shadowRoot!.querySelectorAll('.item');
    assertEquals(3, renderedItems.length);
    assertEquals('Hello', renderedItems[0]!.textContent);
    assertEquals('World', renderedItems[1]!.textContent);
    assertEquals('Goodbye', renderedItems[2]!.textContent);

    // Correctly render no items.
    testElement.reset();
    testElement.items = [];
    itemsRendered = await testElement.allItemsRendered();
    assertEquals(1, itemsRendered.length);
    assertEquals(0, itemsRendered[0]);
    assertEquals(0, testElement.shadowRoot!.querySelectorAll('.item').length);
  });
});
