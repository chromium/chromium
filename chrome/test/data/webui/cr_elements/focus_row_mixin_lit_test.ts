// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {FocusRowMixinLit} from 'chrome://resources/cr_elements/focus_row_mixin_lit.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {down, up} from 'chrome://webui-test/mouse_mock_interactions.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {html, CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {assertFalse, assertTrue, assertEquals} from 'chrome://webui-test/chai_assert.js';

// clang-format on

class ButtonThreeElement extends CrLitElement {
  static get is() {
    return 'button-three';
  }

  override render() {
    return html`
      <button>
        fake button three
      </button>
    `;
  }

  getFocusableElement() {
    return this.shadowRoot!.querySelector('button');
  }
}
customElements.define(ButtonThreeElement.is, ButtonThreeElement);

const TestElementBase = FocusRowMixinLit(CrLitElement);

interface TestFocusRowMixinLitElement {
  $: {
    control: HTMLElement,
    controlTwo: HTMLElement,
  };
}

class TestFocusRowMixinLitElement extends TestElementBase {
  static get is() {
    return 'test-focus-row-mixin-lit';
  }

  static override get properties() {
    return {
      showExtraControl: {type: Boolean},
    };
  }

  override render() {
    // clang-format off
    return html`
      <div id="container" focus-row-container>
        <span>fake text</span>
        <button id="control" focus-row-control focus-type='fake-btn'>
          fake button
        </button>
        <button id="controlTwo" focus-row-control focus-type='fake-btn-two'>
          fake button two
        </button>
        <button-three focus-row-control focus-type='fake-btn-three'>
        </button-three>
        ${this.showExtraControl ? html`<button id="controlFour"
            focus-row-control focus-type='fake-btn-four'>
          fake button four
        </button>` : ''}
      </div>
    `;
    // clang-format on
  }

  showExtraControl: boolean = false;
  focusCallCount: number = 0;

  override focus() {
    this.focusCallCount++;
  }
}
customElements.define(
    TestFocusRowMixinLitElement.is, TestFocusRowMixinLitElement);

declare global {
  interface HTMLElementTagNameMap {
    'test-focus-row-mixin-lit': TestFocusRowMixinLitElement;
  }
}

suite('FocusRowMixinLitTest', function() {
  let testElement: TestFocusRowMixinLitElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testElement = document.createElement('test-focus-row-mixin-lit');
    document.body.appendChild(testElement);
    // Wait to ensure that async setup in FocusRowMixinLit has executed.
    await microtasksFinished();
  });

  test('ID is not overriden when index is set', async () => {
    assertFalse(testElement.hasAttribute('id'));
    assertFalse(testElement.hasAttribute('aria-rowindex'));
    testElement.setAttribute('id', 'test-id');
    assertTrue(testElement.hasAttribute('id'));
    assertEquals('test-id', testElement.id);
    assertFalse(testElement.hasAttribute('aria-rowindex'));
    testElement.focusRowIndex = 5;  // Arbitrary index.
    await microtasksFinished();
    assertTrue(testElement.hasAttribute('id'));
    assertEquals('test-id', testElement.id);
    assertTrue(testElement.hasAttribute('aria-rowindex'));
  });

  test('ID and aria-rowindex are only set when index is set', async () => {
    assertFalse(testElement.hasAttribute('id'));
    assertFalse(testElement.hasAttribute('aria-rowindex'));
    testElement.focusRowIndex = 5;  // Arbitrary index.
    await microtasksFinished();
    assertTrue(testElement.hasAttribute('id'));
    assertTrue(testElement.hasAttribute('aria-rowindex'));
  });

  test('item passes focus to first focusable child', function() {
    let focused = false;
    testElement.$.control.addEventListener('focus', function() {
      focused = true;
    });
    testElement.dispatchEvent(new CustomEvent('focus'));
    assertTrue(focused);
  });

  test('will focus a similar item that was last focused', function() {
    const lastButton = document.createElement('button');
    lastButton.setAttribute('focus-type', 'fake-btn-two');
    testElement.lastFocused = lastButton;

    let focused = false;
    testElement.$.controlTwo.addEventListener('focus', function() {
      focused = true;
    });
    testElement.dispatchEvent(new CustomEvent('focus'));
    assertTrue(focused);
  });

  test('mouse clicks on the row does not focus the controls', function() {
    let focused = false;
    testElement.$.control.addEventListener('focus', function() {
      focused = true;
    });
    down(testElement);
    up(testElement);
    testElement.click();
    // iron-list is responsible for firing 'focus' after taps, but is not used
    // in the test, so its necessary to manually fire 'focus' after tap.
    testElement.dispatchEvent(new CustomEvent('focus'));
    assertFalse(focused);
  });

  test(
      'when focus-override is defined, returned element gains focus',
      async () => {
        const lastButton = document.createElement('button');
        lastButton.setAttribute('focus-type', 'fake-btn-three');
        testElement.lastFocused = lastButton;

        const whenFocus = eventToPromise('focus', testElement);
        testElement.dispatchEvent(new CustomEvent('focus'));
        await whenFocus;
        const button = getDeepActiveElement();
        assertTrue(!!button);
        assertEquals('fake button three', button.textContent!.trim());
      });

  test(
      'when shift+tab pressed on first control, focus on container',
      async () => {
        const first = testElement.$.control;
        const second = testElement.$.controlTwo;
        pressAndReleaseKeyOn(first, 0, 'shift', 'Tab');
        assertEquals(1, testElement.focusCallCount);
        pressAndReleaseKeyOn(second, 0, 'shift', 'Tab');
        assertEquals(1, testElement.focusCallCount);

        // Simulate updating a row with same first control.
        testElement.showExtraControl = true;
        await microtasksFinished();
        pressAndReleaseKeyOn(first, 0, 'shift', 'Tab');
        assertEquals(2, testElement.focusCallCount);
        pressAndReleaseKeyOn(second, 0, 'shift', 'Tab');
        assertEquals(2, testElement.focusCallCount);

        // Simulate updating row with different first control.
        first.remove();
        await microtasksFinished();
        pressAndReleaseKeyOn(second, 0, 'shift', 'Tab');
        assertEquals(3, testElement.focusCallCount);
      });
});
