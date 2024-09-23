// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';

import type {CrPageSelectorElement} from 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('cr-page-selector', () => {
  let element: CrPageSelectorElement;

  setup(() => {
    document.body.innerHTML = getTrustedHTML`
      <cr-page-selector attr-for-selected="path">
        <div id="a" path="a">Page A</div>
        <div id="b" path="b">Page B</div>
        <div id="c" path="c">Page C</div>
      </cr-page-selector>
    `;
    element = document.querySelector('cr-page-selector')!;
  });

  test('Only selected is visible', async() => {
    element.selected = 'a';
    await element.updateComplete;

    assertTrue(!!document.body.querySelector('#a.selected'));
    assertTrue(isVisible(document.body.querySelector('#a')));
    assertFalse(isVisible(document.body.querySelector('#b')));
    assertFalse(isVisible(document.body.querySelector('#c')));

    element.selected = 'c';
    await element.updateComplete;
    assertTrue(!!document.body.querySelector('#c.selected'));
    assertFalse(isVisible(document.body.querySelector('#a')));
    assertFalse(isVisible(document.body.querySelector('#b')));
    assertTrue(isVisible(document.body.querySelector('#c')));
  });

  test('show-all', async () => {
    element.toggleAttribute('show-all', true);
    element.selected = 'a';
    await element.updateComplete;

    assertTrue(!!document.body.querySelector('#a.selected'));
    assertTrue(isVisible(document.body.querySelector('#a')));
    assertTrue(isVisible(document.body.querySelector('#b')));
    assertTrue(isVisible(document.body.querySelector('#c')));

    element.selected = 'c';
    await element.updateComplete;
    assertTrue(!!document.body.querySelector('#c.selected'));
    assertTrue(isVisible(document.body.querySelector('#a')));
    assertTrue(isVisible(document.body.querySelector('#b')));
    assertTrue(isVisible(document.body.querySelector('#c')));
  });

  test('Click does not select', async () => {
    element.selected = 'a';
    await element.updateComplete;

    const pageA = document.body.querySelector<HTMLElement>('#a');
    assertTrue(!!pageA);

    const whenClicked = new Promise<void>(resolve => {
      pageA.addEventListener('click', () => {
        element.selected = 'b';
        resolve();
      });
    });
    pageA.click();

    await whenClicked;
    await element.updateComplete;
    assertEquals('b', element.selected);
  });
});

class TestPageManager extends CrLitElement {
  static get is() {
    return 'test-page-manager';
  }

  static override get properties() {
    return {
      page: {type: String},
    };
  }

  page: string = '';

  override render() {
    return html`<cr-page-selector has-nested-slots attr-for-selected="path"
          .selected="${this.page}">
      <slot name="foo"></slot>
      <slot name="bar"></slot>
      <div id="z" path="z">Page Z</div>
    </cr-page-selector>`;
  }
}

customElements.define('test-page-manager', TestPageManager);

suite('cr-page-selector nested slots', () => {
  let element: TestPageManager;

  setup(() => {
    document.body.innerHTML = getTrustedHTML`
      <test-page-manager>
        <div id="a" path="a" slot="foo">Page A</div>
        <div id="b" path="b" slot="foo">Page B</div>
        <div id="c" path="c" slot="bar">Page C</div>
      </test-page-manager>
    `;
    const manager =
        document.querySelector<TestPageManager>('test-page-manager');
    assertTrue(!!manager);
    element = manager;
  });

  test('Only selected is visible', async () => {
    element.page = 'a';
    await microtasksFinished();

    assertTrue(isVisible(element.querySelector('#a.selected')));
    assertFalse(isVisible(element.querySelector('#b')));
    assertFalse(isVisible(element.querySelector('#c')));
    assertFalse(isVisible(element.shadowRoot!.querySelector('#z')));

    element.page = 'c';
    await microtasksFinished();
    assertTrue(isVisible(element.querySelector('#c.selected')));
    assertFalse(isVisible(element.querySelector('#a')));
    assertFalse(isVisible(element.querySelector('#b')));
    assertFalse(isVisible(element.shadowRoot!.querySelector('#z')));

    element.page = 'z';
    await microtasksFinished();
    assertTrue(isVisible(element.shadowRoot!.querySelector('#z.selected')));
    assertFalse(isVisible(element.querySelector('#a')));
    assertFalse(isVisible(element.querySelector('#b')));
    assertFalse(isVisible(element.querySelector('#c')));

    element.page = 'd';
    await microtasksFinished();
    // Nothing is selected.
    assertFalse(!!element.querySelector('.selected'));
    assertFalse(isVisible(element.querySelector('#a')));
    assertFalse(isVisible(element.querySelector('#b')));
    assertFalse(isVisible(element.querySelector('#c')));
    assertFalse(isVisible(element.shadowRoot!.querySelector('#z')));

    // Make sure the slotchange event for the nested slot triggers
    // iron-items-changed.
    const div = document.createElement('div');
    div.setAttribute('path', 'd');
    div.id = 'd';
    div.slot = 'bar';
    div.textContent = 'Page D';
    element.appendChild(div);
    await eventToPromise('iron-items-changed', element);
    await microtasksFinished();

    // Verify the selected item is now visible.
    assertTrue(isVisible(element.querySelector('#d.selected')));
    assertFalse(isVisible(element.querySelector('#a')));
    assertFalse(isVisible(element.querySelector('#b')));
    assertFalse(isVisible(element.querySelector('#c')));
    assertFalse(isVisible(element.shadowRoot!.querySelector('#z')));

    // Make sure the slotchange event for the non-nested slot triggers
    // iron-items-changed.
    const divY = document.createElement('div');
    divY.setAttribute('path', 'y');
    divY.id = 'y';
    divY.textContent = 'Page Y';
    const selector = element.shadowRoot!.querySelector('cr-page-selector');
    assertTrue(!!selector);
    selector.appendChild(divY);
    await eventToPromise('iron-items-changed', element);

    // Select page Y.
    element.page = 'y';
    await microtasksFinished();

    assertTrue(isVisible(element.shadowRoot!.querySelector('#y.selected')));
    assertFalse(isVisible(element.querySelector('#a')));
    assertFalse(isVisible(element.querySelector('#b')));
    assertFalse(isVisible(element.querySelector('#c')));
    assertFalse(isVisible(element.querySelector('#d')));
    assertFalse(isVisible(element.shadowRoot!.querySelector('#z')));
  });
});
