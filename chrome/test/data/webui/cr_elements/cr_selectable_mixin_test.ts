// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrSelectableMixin} from 'chrome://resources/cr_elements/cr_selectable_mixin.js';
import {html, CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';
import {assertEquals, assertTrue, assertNull} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-scrollable-mixin', function() {
  const TestElementBase = CrSelectableMixin(CrLitElement);

  class TestElement extends TestElementBase {
    static get is() {
      return 'test-element';
    }

    override render() {
      return html`
         <slot></slot>
       `;
    }
  }

  customElements.define(TestElement.is, TestElement);

  let element: TestElement;

  setup(function() {
    document.body.innerHTML = getTrustedHtml(`
      <test-element attr-for-selected="href" selected-attribute="selected"
          selectable="[selectable]">
        <a href="/a" selectable>a</a>
        <a href="/b" selectable>b</a>
        <a href="/c" selectable>c</a>
        <a href="/d">d</a>
      </test-element>`);
    element = document.body.querySelector('test-element')!;
    return element.updateComplete;
  });

  test('updates items', async () => {
    assertEquals(3, element.getItemsForTest().length);

    const newItem = document.createElement('a');
    newItem.setAttribute('href', '/e');
    newItem.textContent = 'e';
    newItem.toggleAttribute('selectable', true);
    let itemsUpdated = eventToPromise('iron-items-changed', element);
    element.appendChild(newItem);
    await itemsUpdated;
    assertEquals(4, element.getItemsForTest().length);

    const newUnselectable = document.createElement('a');
    newUnselectable.setAttribute('href', '/f');
    newUnselectable.textContent = 'f';
    itemsUpdated = eventToPromise('iron-items-changed', element);
    element.appendChild(newUnselectable);
    await itemsUpdated;
    assertEquals(4, element.getItemsForTest().length);

    const c = element.querySelectorAll('a')[2]!;
    itemsUpdated = eventToPromise('iron-items-changed', element);
    element.removeChild(c);
    await itemsUpdated;
    assertEquals(3, element.getItemsForTest().length);
  });

  test('fires events', async () => {
    assertEquals(undefined, element.selected);

    let selectEvent = eventToPromise('iron-select', element);
    let activateEvent = eventToPromise('iron-activate', element);
    const elements = element.querySelectorAll('a');
    elements[0]!.click();
    const events = await Promise.all([activateEvent, selectEvent]);

    assertEquals('/a', events[0]!.detail.selected);
    assertEquals(elements[0], events[0]!.detail.item);
    assertEquals(elements[0], events[1]!.detail.item);
    assertEquals('/a', element.selected);

    selectEvent = eventToPromise('iron-select', element);
    activateEvent = eventToPromise('iron-activate', element);
    const deselectEvent = eventToPromise('iron-deselect', element);
    elements[1]!.click();
    const newEvents =
        await Promise.all([activateEvent, deselectEvent, selectEvent]);

    assertEquals('/b', newEvents[0]!.detail.selected);
    assertEquals(elements[1], newEvents[0]!.detail.item);
    assertEquals(elements[0], newEvents[1]!.detail.item);
    assertEquals(elements[1], newEvents[2]!.detail.item);
    assertEquals('/b', element.selected);
  });

  test('sets attribute and class', async () => {
    function assertSelected(selectedIndex: number) {
      const elements = element.querySelectorAll('a[selectable]');
      for (let index = 0; index < elements.length; index++) {
        assertEquals(
            index === selectedIndex, elements[index]!.hasAttribute('selected'));
        assertEquals(
            index === selectedIndex,
            elements[index]!.classList.contains('selected'));
      }
    }

    element.selected = '/c';
    await eventToPromise('iron-select', element);
    assertSelected(2);

    element.selected = '/a';
    await eventToPromise('iron-select', element);
    assertSelected(0);
  });
});

suite('cr-scrollable-mixin overrides', function() {
  const TestOverridesElementBase = CrSelectableMixin(CrLitElement);

  class TestOverridesElement extends TestOverridesElementBase {
    static get is() {
      return 'test-overrides-element';
    }

    override render() {
      return html`
        <a href="/a">a</a>
        <a href="/b">b</a>
        <a href="/c">c</a>
        <a href="/d">d</a>
        <div>e</div>
      `;
    }

    constructor() {
      super();

      /** Property for CrSelectableMixin */
      this.attrForSelected = 'href';
    }

    // Override `observeItems` from CrSelectableMixin.
    override observeItems() {
      // Turn off default observation logic in CrSelectableMixin.
    }

    // Override `queryItems` from CrSelectableMixin.
    override queryItems() {
      return Array.from(this.shadowRoot!.querySelectorAll('a'));
    }

    // Override `queryMatchingItem` from CrSelectableMixin.
    override queryMatchingItem(selector: string) {
      return this.shadowRoot!.querySelector<HTMLElement>(`a${selector}`);
    }

    override connectedCallback() {
      super.connectedCallback();
      this.itemsChanged();
    }
  }

  customElements.define(TestOverridesElement.is, TestOverridesElement);

  let element: TestOverridesElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('test-overrides-element') as
        TestOverridesElement;
    document.body.appendChild(element);
  });

  test('All', async () => {
    // Assert that selectable items have been detected.
    assertEquals(4, element.getItemsForTest().length);
    assertNull(element.selectedItem);

    // Select the 2nd item.
    element.selected = '/b';
    await element.updateComplete;
    let selectedItem = element.shadowRoot!.querySelector('.selected');
    assertTrue(!!selectedItem);
    assertEquals(selectedItem, element.selectedItem);
    assertEquals('b', selectedItem.textContent);

    // Remove the selected item, and manually call itemsChanged().
    selectedItem.remove();
    element.itemsChanged();
    assertEquals(3, element.getItemsForTest().length);
    assertNull(element.selectedItem);

    // Select the 1st item.
    element.selected = '/a';
    await element.updateComplete;
    selectedItem = element.shadowRoot!.querySelector('.selected');
    assertTrue(!!selectedItem);
    assertEquals(selectedItem, element.selectedItem);
    assertEquals('a', selectedItem.textContent);

    // Select the next item.
    element.selectNext();
    await element.updateComplete;
    assertEquals('/c', element.selected);
    selectedItem = element.shadowRoot!.querySelector('.selected');
    assertTrue(!!selectedItem);
    assertEquals(selectedItem, element.selectedItem);
    assertEquals('c', selectedItem.textContent);
  });
});
