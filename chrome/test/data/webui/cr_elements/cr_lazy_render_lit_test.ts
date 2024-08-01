// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {html, CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('cr-lazy-render', function() {
  // Test parent element.
  class TestElement extends CrLitElement {
    static get is() {
      return 'test-element';
    }

    static override get properties() {
      return {
        name: {type: String},
        checked: {type: Boolean},
      };
    }

    name: string = '';
    checked: boolean = false;

    override render() {
      return html`
        <cr-lazy-render-lit .template=${() => html`
          <h1>
            <cr-checkbox ?checked="${this.checked}"
                @checked-changed="${this.onCheckedChanged_}"></cr-checkbox>
            ${this.name}
          </h1>
        `}></cr-lazy-render-lit>`;
    }

    private onCheckedChanged_(e: CustomEvent<{value: boolean}>) {
      this.checked = e.detail.value;
    }
  }

  customElements.define(TestElement.is, TestElement);

  let lazy: CrLazyRenderLitElement<HTMLElement>;
  let parent: TestElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    parent = document.createElement('test-element') as TestElement;
    document.body.appendChild(parent);
    lazy = parent.shadowRoot!.querySelector('cr-lazy-render-lit')!;
  });

  test('stamps after get()', function() {
    assertFalse(!!parent.shadowRoot!.querySelector('h1'));
    assertFalse(!!lazy.getIfExists());

    const inner = lazy.get();
    assertEquals(inner, parent.shadowRoot!.querySelector('h1'));
    assertEquals(inner, lazy.previousElementSibling);
    assertEquals(lazy.getIfExists(), parent.shadowRoot!.querySelector('h1'));
    assertEquals('H1', inner.nodeName);
    assertEquals(inner, parent.shadowRoot!.querySelector('h1'));
  });

  test('one-way binding works', async function() {
    parent.name = 'Wings';

    const inner = lazy.get();

    const checkbox = parent.shadowRoot!.querySelector('cr-checkbox');
    assertTrue(!!checkbox);
    assertFalse(checkbox.checked);
    assertNotEquals(-1, inner.textContent!.indexOf('Wings'));

    parent.name = 'DC';
    await microtasksFinished();
    assertNotEquals(-1, inner.textContent!.indexOf('DC'));
  });

  test('events work', async function() {
    parent.checked = true;

    lazy.get();
    const checkbox = parent.shadowRoot!.querySelector('cr-checkbox');
    assertTrue(!!checkbox);
    assertTrue(checkbox.checked);

    checkbox.click();
    await microtasksFinished();
    assertFalse(checkbox.checked);
    assertFalse(parent.checked);
  });

  test('SameInstanceAfterRerender', async function() {
    assertFalse(!!lazy.getIfExists());
    const inner1 = lazy.get();
    assertEquals(inner1, parent.shadowRoot!.querySelector('h1'));
    parent.name = 'Something';
    await microtasksFinished();
    const inner2 = lazy.get();
    assertEquals(inner2, parent.shadowRoot!.querySelector('h1'));
    assertEquals(inner1, inner2);
  });
});
