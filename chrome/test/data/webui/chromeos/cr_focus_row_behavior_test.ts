// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {FocusRowBehavior} from 'chrome://resources/ash/common/focus_row_behavior.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {down, pressAndReleaseKeyOn, up} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {html, PolymerElement, mixinBehaviors} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {assertFalse, assertTrue, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

// clang-format on

class ButtonThreeElement extends PolymerElement {
  static get is() {
    return 'button-three';
  }

  static get template() {
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

const TestElementBase = mixinBehaviors([FocusRowBehavior], PolymerElement) as
    {new (): PolymerElement & FocusRowBehavior};

interface TestFocusRowBehaviorElement {
  $: {
    control: HTMLElement,
    controlTwo: HTMLElement,
  };
}

class TestFocusRowBehaviorElement extends TestElementBase {
  static get is() {
    return 'test-focus-row-behavior-element';
  }

  static get template() {
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
      </div>
    `;
  }

  focusCallCount: number = 0;

  override focus() {
    this.focusCallCount++;
  }
}
customElements.define(
    TestFocusRowBehaviorElement.is, TestFocusRowBehaviorElement);

declare global {
  interface HTMLElementTagNameMap {
    'test-focus-row-behavior-element': TestFocusRowBehaviorElement;
  }
}


suite('cr-focus-row-behavior-test', function() {
  let testElement: TestFocusRowBehaviorElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testElement = document.createElement('test-focus-row-behavior-element');
    document.body.appendChild(testElement);

    // Block so that FocusRowBehavior.attached can run.
    await waitAfterNextRender(testElement);
    // Wait one more time to ensure that async setup in FocusRowBehavior has
    // executed.
    await waitAfterNextRender(testElement);
  });

  test('ID is not overriden when index is set', function() {
    assertFalse(testElement.hasAttribute('id'));
    assertFalse(testElement.hasAttribute('aria-rowindex'));
    testElement.id = 'test-id';
    assertTrue(testElement.hasAttribute('id'));
    assertEquals('test-id', testElement.id);
    assertFalse(testElement.hasAttribute('aria-rowindex'));
    testElement.focusRowIndex = 5;  // Arbitrary index.
    assertTrue(testElement.hasAttribute('id'));
    assertEquals('test-id', testElement.id);
    assertTrue(testElement.hasAttribute('aria-rowindex'));
  });

  test('ID and aria-rowindex are only set when index is set', function() {
    assertFalse(testElement.hasAttribute('id'));
    assertFalse(testElement.hasAttribute('aria-rowindex'));
    testElement.focusRowIndex = 5;  // Arbitrary index.
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

  test('when focus-override is defined, returned element gains focus', () => {
    const lastButton = document.createElement('button');
    lastButton.setAttribute('focus-type', 'fake-btn-three');
    testElement.lastFocused = lastButton;

    const wait = eventToPromise('focus', testElement);
    testElement.dispatchEvent(new CustomEvent('focus'));
    return wait.then(() => {
      const button = getDeepActiveElement();
      assertTrue(!!button);
      assertEquals('fake button three', button.textContent!.trim());
    });
  });

  test('when shift+tab pressed on first control, focus on container', () => {
    const first = testElement.$.control;
    const second = testElement.$.controlTwo;
    pressAndReleaseKeyOn(first, 0, 'shift', 'Tab');
    assertEquals(1, testElement.focusCallCount);
    pressAndReleaseKeyOn(second, 0, 'shift', 'Tab');
    assertEquals(1, testElement.focusCallCount);

    // Simulate updating a row with same first control.
    testElement.dispatchEvent(new CustomEvent('dom-change'));
    pressAndReleaseKeyOn(first, 0, 'shift', 'Tab');
    assertEquals(2, testElement.focusCallCount);
    pressAndReleaseKeyOn(second, 0, 'shift', 'Tab');
    assertEquals(2, testElement.focusCallCount);

    // Simulate updating row with different first control.
    first.remove();
    testElement.dispatchEvent(new CustomEvent('dom-change'));
    pressAndReleaseKeyOn(second, 0, 'shift', 'Tab');
    assertEquals(3, testElement.focusCallCount);
  });
});
