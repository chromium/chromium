// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

interface CrDummyLitElement {
  $: {
    foo: HTMLElement,
    bar: HTMLElement,
  };
}

class CrDummyLitElement extends CrLitElement {
  static get is() {
    return 'cr-dummy-lit';
  }

  override render() {
    return html`
      <div id="foo">Hello Foo</div>
      <div id="bar">Hello Bar</div>
    `;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-dummy-lit': CrDummyLitElement;
  }
}

customElements.define(CrDummyLitElement.is, CrDummyLitElement);


suite('CrLitElement', function() {
  test('DollarSign', async function() {
    const element = document.createElement('cr-dummy-lit');
    document.body.appendChild(element);
    await element.updateComplete;

    assertTrue(!!element.$.foo);
    assertEquals('Hello Foo', element.$.foo.textContent);
    assertTrue(!!element.$.bar);
    assertEquals('Hello Bar', element.$.bar.textContent);
  });
});
