// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
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

  static override get properties() {
    return {
      fooBarBoolean: {type: Boolean},
      fooBarString: {type: String},

      fooBarStringCustom: {
        attribute: 'foobarstringcustom',
        type: String,
      },
    };
  }

  fooBarBoolean: boolean = false;
  fooBarString: string = 'hello';
  fooBarStringCustom: string = 'hola';

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
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('DollarSign', async function() {
    const element = document.createElement('cr-dummy-lit');
    document.body.appendChild(element);
    await element.updateComplete;

    assertTrue(!!element.$.foo);
    assertEquals('Hello Foo', element.$.foo.textContent);
    assertTrue(!!element.$.bar);
    assertEquals('Hello Bar', element.$.bar.textContent);
  });

  // Test that properties are initialized correctly from attributes.
  test('PropertiesAttributesNameMapping', function() {
    document.body.innerHTML = getTrustedHTML`
      <cr-dummy-lit foo-bar-boolean foo-bar-string="world"
          foobarstringcustom="custom">
      </cr-dummy-lit>
    `;

    const element = document.body.querySelector('cr-dummy-lit');
    assertTrue(!!element);
    assertTrue(element.fooBarBoolean);
    assertEquals('world', element.fooBarString);
    assertEquals('custom', element.fooBarStringCustom);
  });
});
