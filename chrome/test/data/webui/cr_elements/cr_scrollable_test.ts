// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCss as getCrScrollableCss} from 'chrome://resources/cr_elements/cr_scrollable_lit.css.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

class TestElement extends CrLitElement {
  static get is() {
    return 'test-element';
  }

  static override get styles() {
    return getCrScrollableCss();
  }

  override render() {
    return html`
      <div class="cr-scrollable" style="width: 500px; height: 200px;
          --cr-scrollable-border-color: rgb(255, 0, 0);">
        <div class="cr-scrollable-top"></div>
        <div class="block" style="width: 500px; height: 100px;"></div>
        <div class="cr-scrollable-bottom"></div>
      </div>
    `;
  }
}
customElements.define('test-element', TestElement);

suite('cr-scrollable', () => {
  let testElement: TestElement;

  let scrollableElement: HTMLElement;
  let childBlockElement: HTMLElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);

    scrollableElement = testElement.shadowRoot.querySelector('.cr-scrollable')!;
    childBlockElement = scrollableElement.querySelector('.block')!;
  });

  function waitForElementVisibility(element: HTMLElement): Promise<void> {
    assertFalse(isVisible(element));
    return new Promise(resolve => {
      const resizeObserver = new ResizeObserver(() => {
        if (isVisible(element)) {
          resizeObserver.unobserve(element);
          resolve();
        }
      });
      resizeObserver.observe(element);
    });
  }

  test('ShowsTopBorder', async () => {
    const topBorder =
        scrollableElement.querySelector<HTMLElement>('.cr-scrollable-top')!;
    assertFalse(isVisible(topBorder));

    // Make the child block very tall to make the parent scrollable and then
    // scroll some, making the top border visible.
    childBlockElement.style.height = '400px';
    scrollableElement.scrollTop = 50;
    await waitForElementVisibility(topBorder);
    assertEquals(scrollableElement.offsetLeft, topBorder.offsetLeft);
    assertEquals(scrollableElement.offsetTop, topBorder.offsetTop);
    assertEquals(scrollableElement.offsetWidth, topBorder.offsetWidth);
    assertEquals('rgb(255, 0, 0)', getComputedStyle(topBorder).borderTopColor);
  });

  test('ShowsBottomBorder', async () => {
    const bottomBorder =
        scrollableElement.querySelector<HTMLElement>('.cr-scrollable-bottom')!;
    assertFalse(isVisible(bottomBorder));

    // Make the child block very tall to make the parent scrollable, making the
    // bottom border visible.
    childBlockElement.style.height = '400px';
    await waitForElementVisibility(bottomBorder);
    assertEquals(scrollableElement.offsetLeft, bottomBorder.offsetLeft);
    assertEquals(
        scrollableElement.offsetTop + scrollableElement.offsetHeight -
            bottomBorder.offsetHeight,
        bottomBorder.offsetTop);
    assertEquals(scrollableElement.offsetWidth, bottomBorder.offsetWidth);
    assertEquals(
        'rgb(255, 0, 0)', getComputedStyle(bottomBorder).borderBottomColor);

    // Scrolling to the very bottom should hide the bottom border.
    const scrollPromise = eventToPromise('scroll', scrollableElement);
    scrollableElement.scrollTop = 20000;
    await scrollPromise;
    assertFalse(isVisible(bottomBorder));
  });
});
