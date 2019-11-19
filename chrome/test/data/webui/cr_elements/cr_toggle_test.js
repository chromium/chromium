// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
// #import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// #import {eventToPromise} from '../test_util.m.js';
// clang-format on

suite('cr-toggle', function() {
  let toggle;

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-toggle id="toggle"></cr-toggle>
    `;

    toggle = document.getElementById('toggle');
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
   * @param {string} keyName The name of the key to trigger.
   * @param {string} keyCode The event keyCode and code to trigger.
   */
  function triggerKeyPressEvent(keyName, keyCode) {
    // Note: MockInteractions incorrectly populates |keyCode| and |code| with
    // the same value. The intention of passing a string here is only to set
    // |code|, since |keyCode| is not used its value doesn't matter.
    MockInteractions.keyEventOn(
        toggle, 'keypress', keyCode, undefined, keyName);
  }

  /**
   * Simulates dragging the toggle button left/right.
   * @param {number} moveDirection -1 for left, 1 for right, 0 when no
   *     pointermove event should be simulated.
   * @param {number=} diff The move amount in pixels. Only relevant if
   *     moveDirection is non-zero.
   */
  function triggerPointerDownMoveUpTapSequence(moveDirection, diff) {
    if (window.getComputedStyle(toggle)['pointer-events'] === 'none') {
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
      xEnd = moveDirection > 0 ? xStart + diff : xStart - diff;
      toggle.dispatchEvent(
          new PointerEvent('pointermove', {pointerId: 1, clientX: xEnd}));
    }
    toggle.dispatchEvent(
        new PointerEvent('pointerup', {pointerId: 1, clientX: xEnd}));
    toggle.click();
  }

  // Test that the control is toggled when the |checked| attribute is
  // programmatically changed.
  test('ToggleByAttribute', function() {
    test_util.eventToPromise('change', toggle).then(function() {
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
    let whenChanged = test_util.eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(0 /* no pointermove */);
    return whenChanged
        .then(function() {
          assertChecked();
          whenChanged = test_util.eventToPromise('change', toggle);
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
    let whenChanged = test_util.eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(
        1 /* right */, toggle.MOVE_THRESHOLD_PX - 1);
    return whenChanged
        .then(function() {
          assertChecked();
          whenChanged = test_util.eventToPromise('change', toggle);
          triggerPointerDownMoveUpTapSequence(
              1 /* right */, toggle.MOVE_THRESHOLD_PX - 1);
          return whenChanged;
        })
        .then(function() {
          assertNotChecked();
        });
  });

  // Test that the control is toggled when the user moves the pointer while
  // holding down.
  test('ToggleByPointerMove', function() {
    let whenChanged = test_util.eventToPromise('change', toggle);
    triggerPointerDownMoveUpTapSequence(
        1 /* right */, toggle.MOVE_THRESHOLD_PX);
    return whenChanged
        .then(function() {
          assertChecked();
          whenChanged = test_util.eventToPromise('change', toggle);
          triggerPointerDownMoveUpTapSequence(
              -1 /* left */, toggle.MOVE_THRESHOLD_PX);
          return whenChanged;
        })
        .then(function() {
          assertNotChecked();
          whenChanged = test_util.eventToPromise('change', toggle);

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
  test('ToggleByKey', function() {
    let whenChanged = test_util.eventToPromise('change', toggle);
    triggerKeyPressEvent('Enter', 'Enter');
    return whenChanged
        .then(function() {
          assertChecked();
          whenChanged = test_util.eventToPromise('change', toggle);
          triggerKeyPressEvent(' ', 'Space');
          return whenChanged;
        })
        .then(function() {
          assertNotChecked();
          whenChanged = test_util.eventToPromise('change', toggle);
          triggerKeyPressEvent('Enter', 'NumpadEnter');
          return whenChanged;
        })
        .then(function() {
          assertChecked();
        });
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
