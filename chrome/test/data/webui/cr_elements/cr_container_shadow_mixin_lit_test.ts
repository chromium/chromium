// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrContainerShadowMixinLit} from 'chrome://resources/cr_elements/cr_container_shadow_mixin_lit.js';
import {CrLitElement, html, css} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

suite('CrContainerShadowMixinLit', function() {
  const TestElementBase = CrContainerShadowMixinLit(CrLitElement);

  class TestElement extends TestElementBase {
    static get is() {
      return 'test-element';
    }

    static override get styles() {
      return css`
          #container {
            height: 50px;
            overflow: auto;
            width: 100%;
          }

          #content {
            height: 200%;
            width: 100%;
          }
      `;
    }

    override render() {
      return html`
         <div id="before"></div>
         <div id="container" ?show-bottom-shadow="${this.showBottomShadow}">
           <div id="content"></div>
         </div>
         <div id="after"></div>
       `;
    }

    static override get properties() {
      return {
        showBottomShadow: {type: Boolean},
      };
    }

    showBottomShadow: boolean = false;
  }

  customElements.define(TestElement.is, TestElement);

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('no bottom shadow', async function() {
    const element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);

    // Should not have a bottom shadow div.
    assertFalse(
        !!element.shadowRoot!.querySelector('#cr-container-shadow-bottom'));
    assertTrue(!!element.shadowRoot!.querySelector('#cr-container-shadow-top'));

    element.showBottomShadow = true;
    await element.updateComplete;

    // Still no bottom shadow since this is only checked in connectedCallback();
    assertFalse(
        !!element.shadowRoot!.querySelector('#cr-container-shadow-bottom'));
    assertTrue(!!element.shadowRoot!.querySelector('#cr-container-shadow-top'));
  });

  test('show bottom shadow', function() {
    const element = document.createElement('test-element') as TestElement;
    element.showBottomShadow = true;
    document.body.appendChild(element);

    // Has both shadows.
    assertTrue(
        !!element.shadowRoot!.querySelector('#cr-container-shadow-bottom'));
    assertTrue(!!element.shadowRoot!.querySelector('#cr-container-shadow-top'));
  });
});
