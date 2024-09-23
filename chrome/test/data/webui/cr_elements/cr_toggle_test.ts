// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {MOVE_THRESHOLD_PX} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('cr-toggle', function() {
  let toggle: CrToggleElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toggle = document.createElement('cr-toggle');
    document.body.appendChild(toggle);
    assertNotChecked();
  });

  function assertChecked() {
    assertTrue(toggle.checked);
    assertTrue(toggle.hasAttribute('checked'));
    assertEquals('true', toggle.getAttribute('aria-pressed'));
  }

  function assertNotChecked() {
    assertFalse(toggle.checked);
    assertEquals(null, toggle.getAttribute('checked'));
    assertEquals('false', toggle.getAttribute('aria-pressed'));
  }

  function assertDisabled() {
    assertTrue(toggle.disabled);
    assertEquals('-1', toggle.getAttribute('tabindex'));
    assertTrue(toggle.hasAttribute('disabled'));
    assertEquals('true', toggle.getAttribute('aria-disabled'));
  }

  function assertNotDisabled() {
    assertFalse(toggle.disabled);
    assertEquals('0', toggle.getAttribute('tabindex'));
    assertFalse(toggle.hasAttribute('disabled'));
    assertEquals('false', toggle.getAttribute('aria-disabled'));
  }

  /**
   * Simulates dragging the toggle button left/right.
   * @param moveDirection -1 for left, 1 for right, 0 when no pointermove event
   *     should be simulated.
   * @param diff The move amount in pixels. Only relevant if moveDirection is
   *     non-zero.
   */
  function triggerPointerDownMoveUpTapSequence(
      moveDirection: -1|0|1, diff?: number) {
    const computedStyles = window.getComputedStyle(toggle);
    if (computedStyles.getPropertyValue('pointer-events') === 'none') {
      return;
    }

    // Simulate events in the same order they are fired by the browser.
    // Need to provide a valid |pointerId| for setPointerCapture() to not throw
    // an error.
    const xStart = 100;
    toggle.dispatchEvent(
        new PointerEvent('pointerdown', {pointerId: 1, clientX: xStart}));
    let xEnd = xStart;
    if (moveDirection) {
      xEnd = moveDirection > 0 ? xStart + diff! : xStart - diff!;
      toggle.dispatchEvent(
          new PointerEvent('pointermove', {pointerId: 1, clientX: xEnd}));
    }
    toggle.dispatchEvent(
        new PointerEvent('pointerup', {pointerId: 1, clientX: xEnd}));
    toggle.click();
  }

  // Check if setting checked in HTML works, has to use a separate element to
  // ensure that we are testing brand new state.
  test('initiallyCheckedWorks', function() {
    document.body.innerHTML = getTrustedHTML`<cr-toggle checked></cr-toggle> `;
    toggle = (document.querySelector('cr-toggle'))!;
    assertChecked();
  });

  // Test that the control is toggled when the |checked| attribute is
  // programmatically changed.
  test('ToggleByAttribute', async function() {
    eventToPromise('change', toggle).then(function() {
      // Should not fire 'change' event when state is changed programmatically.
      // Only user interaction should result in 'change' event.
      assertFalse(true);
    });

    toggle.checked = true;
    await toggle.updateComplete;
    assertChecked();

    toggle.checked = false;
    await toggle.updateComplete;
    assertNotChecked();
  });

  // Test that the control is toggled when the user taps on it (no movement
  // between pointerdown and pointerup).
  test('ToggleByPointerTap', async function() {
    let whenChanged = eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    await whenChanged;
    assertChecked();
    whenChanged = eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    await whenChanged;
    assertNotChecked();
  });

  // Test that the control is toggled if the user moves the pointer by a
  // MOVE_THRESHOLD_PX pixels accidentally (shaky hands) in any direction.
  test('ToggleByShakyPointerTap', async function() {
    let whenChanged = eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(1 /* right */, MOVE_THRESHOLD_PX - 1);
    await whenChanged;
    assertChecked();
    whenChanged = eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(1 /* right */, MOVE_THRESHOLD_PX - 1);
    await whenChanged;
    assertNotChecked();
  });

  // Test that the control is toggled when the user moves the pointer while
  // holding down.
  test('ToggleByPointerMove', async function() {
    let whenChanged = eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(1 /* right */, MOVE_THRESHOLD_PX);
    await whenChanged;
    assertChecked();
    whenChanged = eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(-1 /* left */, MOVE_THRESHOLD_PX);
    await whenChanged;
    assertNotChecked();
    whenChanged = eventToPromise('change', toggle);

    // Test simple tapping after having dragged.
    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    await whenChanged;
    assertChecked();
  });

  // Test that the control is toggled when the user presses the 'Enter' or
  // 'Space' key.
  test('ToggleByKey', async () => {
    assertNotChecked();

    toggle.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', repeat: true}));
    await toggle.updateComplete;
    assertNotChecked();

    toggle.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    await toggle.updateComplete;
    assertNotChecked();

    toggle.dispatchEvent(
        new KeyboardEvent('keydown', {key: ' ', repeat: true}));
    await toggle.updateComplete;
    assertNotChecked();

    toggle.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await toggle.updateComplete;
    assertChecked();

    toggle.dispatchEvent(new KeyboardEvent('keyup', {key: ' '}));
    await toggle.updateComplete;
    assertNotChecked();
  });

  // Test that the control is not affected by user interaction when disabled.
  test('ToggleWhenDisabled', async function() {
    assertNotDisabled();
    toggle.disabled = true;
    await toggle.updateComplete;
    assertDisabled();

    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    await toggle.updateComplete;
    assertNotChecked();
    assertDisabled();

    toggle.disabled = false;
    await toggle.updateComplete;
    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    await toggle.updateComplete;
    assertChecked();
  });

  // Test that the control works as expected when the click() method is called.
  test('ToggleWhenWithClick', async function() {
    assertNotDisabled();
    assertNotChecked();

    // State should change because control is enabled.
    toggle.click();
    await toggle.updateComplete;
    assertChecked();

    // State should *not* change because control is disabled.
    toggle.disabled = true;
    await toggle.updateComplete;
    assertDisabled();

    toggle.click();
    await toggle.updateComplete;
    assertChecked();
  });

  // Test that 2-way bindings with Polymer parent elements are updated before
  // the 'change' event is fired.
  test('TwoWayBindingWithPolymerParent', function(done) {
    class TestElement extends PolymerElement {
      static get is() {
        return 'test-element';
      }

      static get template() {
        return html`
          <cr-toggle checked="{{parentChecked}}"
              on-change="onChange"
              on-checked-changed="onCheckedChanged">
          </cr-toggle>`;
      }

      static get properties() {
        return {
          parentChecked: Boolean,
        };
      }

      parentChecked: boolean = false;
      private events_: string[] = [];

      onCheckedChanged(e: CustomEvent<{value: boolean}>) {
        assertEquals(this.events_.length === 0 ? false : true, e.detail.value);
        this.events_.push(e.type);
        this.checkIfFinished_();
      }

      onChange(e: CustomEvent<boolean>) {
        assertTrue(e.detail);
        assertEquals(e.detail, element.parentChecked);
        this.events_.push(e.type);
        this.checkIfFinished_();
      }

      private checkIfFinished_() {
        if (this.events_.length !== 3) {
          return;
        }

        assertDeepEquals(
            ['checked-changed', 'checked-changed', 'change'], this.events_);
        done();
      }
    }

    customElements.define(TestElement.is, TestElement);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);

    const toggle = element.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggle);
    toggle.click();
  });

  test('cssPositionOfKnob', async () => {
    // Disable transitions for tests on the #knob to test accurate pixels.
    toggle.$.knob.style.transition = 'none';

    // Distance between center of knob and left edge of toggle should be
    // --cr-toggle-knob-center-edge-distance_ (8).
    let toggleBounds = toggle.getBoundingClientRect();
    let knobBounds = toggle.$.knob.getBoundingClientRect();
    let knobCenterDistance =
        (knobBounds.left + knobBounds.width / 2) - toggleBounds.left;
    assertEquals(8, knobCenterDistance);

    toggle.click();
    await toggle.updateComplete;

    // Distance between center of knob and right edge of toggle should be
    // --cr-toggle-knob-center-edge-distance_ (8).
    toggleBounds = toggle.getBoundingClientRect();
    knobBounds = toggle.$.knob.getBoundingClientRect();
    knobCenterDistance =
        toggleBounds.right - (knobBounds.left + knobBounds.width / 2);
    assertEquals(8, knobCenterDistance);
  });
});
