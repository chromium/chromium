// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {CrToggleElement, MOVE_THRESHOLD_PX} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
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
    // Asserting that the toggle button has actually moved.
    assertTrue(getComputedStyle(toggle.$.knob).transform.includes('matrix'));
  }

  function assertNotChecked() {
    assertFalse(toggle.checked);
    assertEquals(null, toggle.getAttribute('checked'));
    assertEquals('false', toggle.getAttribute('aria-pressed'));
    // Asserting that the toggle button has not moved.
    assertEquals('none', getComputedStyle(toggle.$.knob).transform);
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
  test('ToggleByAttribute', function() {
    eventToPromise('change', toggle).then(function() {
      // Should not fire 'change' event when state is changed programmatically.
      // Only user interaction should result in 'change' event.
      assertFalse(true);
    });

    toggle.checked = true;
    assertChecked();

    toggle.checked = false;
    assertNotChecked();
  });

  // Test that the control is toggled when the user taps on it (no movement
  // between pointerdown and pointerup).
  test('ToggleByPointerTap', function() {
    let whenChanged = eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    return whenChanged
        .then(function() {
          assertChecked();
          whenChanged = eventToPromise('change', toggle);
          triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
          return whenChanged;
        })
        .then(function() {
          assertNotChecked();
        });
  });

  // Test that the control is toggled if the user moves the pointer by a
  // MOVE_THRESHOLD_PX pixels accidentally (shaky hands) in any direction.
  test('ToggleByShakyPointerTap', function() {
    let whenChanged = eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(1 /* right */, MOVE_THRESHOLD_PX - 1);
    return whenChanged
        .then(function() {
          assertChecked();
          whenChanged = eventToPromise('change', toggle);
          triggerPointerDownMoveUpTapSequence(
              1 /* right */, MOVE_THRESHOLD_PX - 1);
          return whenChanged;
        })
        .then(function() {
          assertNotChecked();
        });
  });

  // Test that the control is toggled when the user moves the pointer while
  // holding down.
  test('ToggleByPointerMove', function() {
    let whenChanged = eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(1 /* right */, MOVE_THRESHOLD_PX);
    return whenChanged
        .then(function() {
          assertChecked();
          whenChanged = eventToPromise('change', toggle);
          triggerPointerDownMoveUpTapSequence(-1 /* left */, MOVE_THRESHOLD_PX);
          return whenChanged;
        })
        .then(function() {
          assertNotChecked();
          whenChanged = eventToPromise('change', toggle);

          // Test simple tapping after having dragged.
          triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
          return whenChanged;
        })
        .then(function() {
          assertChecked();
        });
  });

  // Test that the control is toggled when the user presses the 'Enter' or
  // 'Space' key.
  test('ToggleByKey', () => {
    assertNotChecked();
    toggle.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', repeat: true}));
    assertNotChecked();
    toggle.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    assertNotChecked();
    toggle.dispatchEvent(
        new KeyboardEvent('keydown', {key: ' ', repeat: true}));
    assertNotChecked();
    toggle.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    assertChecked();
    toggle.dispatchEvent(new KeyboardEvent('keyup', {key: ' '}));
    assertNotChecked();
  });

  // Test that the control is not affected by user interaction when disabled.
  test('ToggleWhenDisabled', function() {
    assertNotDisabled();
    toggle.disabled = true;
    assertDisabled();

    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    assertNotChecked();
    assertDisabled();

    toggle.disabled = false;
    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    assertChecked();
  });

  // Test that the control works as expected when the click() method is called.
  test('ToggleWhenWithClick', function() {
    assertNotDisabled();
    assertNotChecked();

    // State should change because control is enabled.
    toggle.click();
    assertChecked();

    // State should *not* change because control is disabled.
    toggle.disabled = true;
    assertDisabled();
    toggle.click();
    assertChecked();
  });
});
