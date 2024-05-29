// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrScrollObserverMixin} from 'chrome://resources/cr_elements/cr_scroll_observer_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {whenAttributeIs} from 'chrome://webui-test/test_util.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

suite('CrScrollObserverMixin', function() {
  const TestElementBase = CrScrollObserverMixin(PolymerElement);

  class TestElement extends TestElementBase {
    static get is() {
      return 'test-element';
    }

    static get template() {
      return html`
         <style>
           #container {
             height: 50px;
             overflow: auto;
             width: 100%;
           }

           #content {
             height: 200%;
             width: 100%;
           }
         </style>
         <div id="before"></div>
         <div id="container"><div id="content"></div></div>
         <div id="after"></div>
       `;
    }
  }

  customElements.define(TestElement.is, TestElement);

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('resize', async () => {
    const element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);

    const container =
        element.shadowRoot!.querySelector<HTMLElement>('#container');
    assertTrue(!!container);

    // Can scroll since content height is 200%.
    await whenAttributeIs(container, 'class', 'scrolled-to-top can-scroll');

    const content = element.shadowRoot!.querySelector<HTMLElement>('#content');
    assertTrue(!!content);

    // Make content half the height of the container. can-scroll should be
    // removed.
    content.style.maxHeight = '25px';
    await whenAttributeIs(
        container, 'class', 'scrolled-to-top scrolled-to-bottom');

    // Make content twice as high again
    content.style.maxHeight = '100px';
    await whenAttributeIs(container, 'class', 'scrolled-to-top can-scroll');
  });

  test('scrolling', async () => {
    const element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);

    const container =
        element.shadowRoot!.querySelector<HTMLElement>('#container');
    assertTrue(!!container);

    // At the top.
    await whenAttributeIs(container, 'class', 'scrolled-to-top can-scroll');

    // Scroll to the middle.
    container.scrollTop = 25;
    await whenAttributeIs(container, 'class', 'can-scroll');

    // Scroll to the bottom.
    container.scrollTop = 50;
    await whenAttributeIs(container, 'class', 'can-scroll scrolled-to-bottom');

    // Back to the middle.
    container.scrollTop = 25;
    await whenAttributeIs(container, 'class', 'can-scroll');

    // Back to the top.
    container.scrollTop = 0;
    await whenAttributeIs(container, 'class', 'can-scroll scrolled-to-top');
  });
});
