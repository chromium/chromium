// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrRippleMixin} from 'chrome://resources/cr_elements/cr_ripple/cr_ripple_mixin.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertNotEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

suite('CrRippleMixin', function() {
  const TestElementBase = CrRippleMixin(CrLitElement);

  class TestElement extends TestElementBase {
    static get is() {
      return 'test-element';
    }

    useCustomContainer: boolean = false;

    override render() {
      return html`
         <div id="container"></div>
       `;
    }

    override createRipple() {
      if (!this.useCustomContainer) {
        return super.createRipple();
      }

      const rippleContainer =
          this.shadowRoot!.querySelector<HTMLElement>('#container');
      assertTrue(!!rippleContainer);
      this.rippleContainer = rippleContainer;
      return super.createRipple();
    }
  }

  customElements.define(TestElement.is, TestElement);

  let element: TestElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);
  });

  test('createRippleDefault', function() {
    assertFalse(element.hasRipple());
    assertEquals(null, element.shadowRoot!.querySelector('cr-ripple'));
    assertEquals(null, element.shadowRoot!.querySelector('#ink'));

    element.ensureRipple();

    assertTrue(element.hasRipple());
    const ripple = element.getRipple();
    assertEquals(ripple, element.shadowRoot!.querySelector('cr-ripple'));
    assertEquals(ripple, element.shadowRoot!.querySelector('#ink'));
    assertEquals(element.shadowRoot, ripple.parentNode);
  });

  test('createRippleOverride', function() {
    assertFalse(element.hasRipple());
    assertEquals(null, element.shadowRoot!.querySelector('cr-ripple'));
    assertEquals(null, element.shadowRoot!.querySelector('#ink'));

    element.useCustomContainer = true;
    element.ensureRipple();

    assertTrue(element.hasRipple());
    assertNotEquals(null, element.shadowRoot!.querySelector('cr-ripple'));
    const ripple = element.getRipple();
    assertEquals(ripple, element.shadowRoot!.querySelector('cr-ripple'));
    assertEquals(ripple, element.shadowRoot!.querySelector('#ink'));
    assertEquals(
        element.shadowRoot!.querySelector('#container'), ripple.parentNode);
  });

  test('noink', async function() {
    assertFalse(element.noink);
    assertFalse(element.hasRipple());
    element.ensureRipple();

    await element.updateComplete;
    assertFalse(element.getRipple().noink);

    element.noink = true;
    await element.updateComplete;
    assertTrue(element.getRipple().noink);
  });
});
