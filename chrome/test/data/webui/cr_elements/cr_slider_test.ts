// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import type {CrSliderElement} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {pressAndReleaseKeyOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {assertEquals, assertFalse, assertTrue, assertNotReached} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-slider', function() {
  let crSlider: CrSliderElement;

  setup(function() {
    document.body.innerHTML = getTrustedHTML`
      <style>
        #wrapper {
          width: 200px;
        }
      </style>
      <div id="wrapper">
        <cr-slider min="0" max="100"></cr-slider>
      </div>
    `;

    crSlider = document.body.querySelector('cr-slider')!;
    crSlider.value = 0;
    return microtasksFinished();
  });

  function checkDisabled(expected: boolean) {
    assertEquals(
        expected,
        window.getComputedStyle(crSlider).getPropertyValue('pointer-events') ===
            'none');
    const expectedTabindex = expected ? '-1' : '0';
    assertEquals(expectedTabindex, crSlider.getAttribute('tabindex'));
  }

  function pressArrowRight() {
    pressAndReleaseKeyOn(crSlider, 39, [], 'ArrowRight');
  }

  function pressArrowLeft() {
    pressAndReleaseKeyOn(crSlider, 37, [], 'ArrowLeft');
  }

  function pressPageUp() {
    pressAndReleaseKeyOn(crSlider, 33, [], 'PageUp');
  }

  function pressPageDown() {
    pressAndReleaseKeyOn(crSlider, 34, [], 'PageDown');
  }

  function pressArrowUp() {
    pressAndReleaseKeyOn(crSlider, 38, [], 'ArrowUp');
  }

  function pressArrowDown() {
    pressAndReleaseKeyOn(crSlider, 40, [], 'ArrowDown');
  }

  function pressHome() {
    pressAndReleaseKeyOn(crSlider, 36, [], 'Home');
  }

  function pressEnd() {
    pressAndReleaseKeyOn(crSlider, 35, [], 'End');
  }

  function pointerEvent(eventType: string, ratio: number) {
    const rect = crSlider.$.container.getBoundingClientRect();
    crSlider.dispatchEvent(new PointerEvent(eventType, {
      buttons: 1,
      pointerId: 1,
      clientX: rect.left + (ratio * rect.width),
    }));
  }

  function pointerDown(ratio: number) {
    pointerEvent('pointerdown', ratio);
  }

  function pointerMove(ratio: number) {
    pointerEvent('pointermove', ratio);
  }

  function pointerUp() {
    // Ignores clientX for pointerup event.
    pointerEvent('pointerup', 0);
  }

  test('key events', () => {
    pressArrowRight();
    assertEquals(1, crSlider.value);
    pressPageUp();
    assertEquals(2, crSlider.value);
    pressArrowUp();
    assertEquals(3, crSlider.value);
    pressHome();
    assertEquals(0, crSlider.value);
    pressArrowLeft();
    assertEquals(0, crSlider.value);
    pressArrowDown();
    assertEquals(0, crSlider.value);
    pressPageDown();
    assertEquals(0, crSlider.value);
    pressEnd();
    assertEquals(100, crSlider.value);
    pressArrowRight();
    assertEquals(100, crSlider.value);
    pressPageUp();
    assertEquals(100, crSlider.value);
    pressArrowUp();
    assertEquals(100, crSlider.value);
    pressArrowLeft();
    assertEquals(99, crSlider.value);
    pressArrowDown();
    assertEquals(98, crSlider.value);
    pressPageDown();
    assertEquals(97, crSlider.value);
  });

  test('key events with key down intervals', async () => {
    crSlider.keyPressSliderIncrement = 10;
    await microtasksFinished();

    pressArrowRight();
    assertEquals(10, crSlider.value);
    pressPageUp();
    assertEquals(20, crSlider.value);
    pressArrowUp();
    assertEquals(30, crSlider.value);
    pressHome();
    assertEquals(0, crSlider.value);
    pressArrowLeft();
    assertEquals(0, crSlider.value);
    pressArrowDown();
    assertEquals(0, crSlider.value);
    pressPageDown();
    assertEquals(0, crSlider.value);
    pressEnd();
    assertEquals(100, crSlider.value);
    pressArrowRight();
    assertEquals(100, crSlider.value);
    pressPageUp();
    assertEquals(100, crSlider.value);
    pressArrowUp();
    assertEquals(100, crSlider.value);
    pressArrowLeft();
    assertEquals(90, crSlider.value);
    pressArrowDown();
    assertEquals(80, crSlider.value);
    pressPageDown();
    assertEquals(70, crSlider.value);

    // Verify value stays within bounds.
    crSlider.value = 98;
    pressArrowRight();
    assertEquals(100, crSlider.value);
    crSlider.value = 2;
    pressArrowLeft();
    assertEquals(0, crSlider.value);
  });

  test('no-keybindings', () => {
    crSlider.noKeybindings = true;
    pressArrowRight();
    assertEquals(0, crSlider.value);
    crSlider.noKeybindings = false;
    pressArrowRight();
    assertEquals(1, crSlider.value);
    crSlider.noKeybindings = true;
    pressArrowRight();
    assertEquals(1, crSlider.value);
    crSlider.noKeybindings = false;
    pressArrowRight();
    assertEquals(2, crSlider.value);
  });

  test('mouse events', () => {
    assertFalse(crSlider.dragging);
    pointerMove(.25);
    assertEquals(0, crSlider.value);
    pointerDown(.5);
    assertTrue(crSlider.dragging);
    assertEquals(50, crSlider.value);
    pointerMove(.75);
    assertEquals(75, crSlider.value);
    pointerMove(-1);
    assertEquals(0, crSlider.value);
    pointerMove(2);
    assertEquals(100, crSlider.value);
    pointerUp();
    assertEquals(100, crSlider.value);
    assertFalse(crSlider.dragging);
    pointerMove(.25);
    assertEquals(100, crSlider.value);
  });

  test('snaps to closest value after minimum traversal', () => {
    // Snaps to closest value after traversing a minimum of .8 units.
    const tolerance = .000001;
    crSlider.snaps = true;
    crSlider.ticks = [];
    pointerDown(.501);
    assertEquals(50, crSlider.value);
    pointerMove(.505);
    assertEquals(50, crSlider.value);
    // Before threshold.
    pointerMove(.508 - tolerance);
    assertEquals(50, crSlider.value);
    // After threshold.
    pointerMove(.508 + tolerance);
    assertEquals(51, crSlider.value);
    // Before threshold.
    pointerMove(.502 + tolerance);
    assertEquals(51, crSlider.value);
    // After threshold.
    pointerMove(.502 - tolerance);
    assertEquals(50, crSlider.value);
    // Move far away rounds to closest whole number.
    pointerMove(.605);
    assertEquals(61, crSlider.value);
  });

  test('markers', async () => {
    const markersElement =
        crSlider.shadowRoot!.querySelector<HTMLElement>('#markers')!;
    assertTrue(markersElement.hidden);
    crSlider.markerCount = 10;
    await microtasksFinished();
    assertFalse(markersElement.hidden);
    const markers =
        Array.from(crSlider.shadowRoot!.querySelectorAll('#markers div'));
    assertEquals(9, markers.length);
    markers.forEach(marker => {
      assertTrue(marker.classList.contains('inactive-marker'));
    });
    crSlider.value = 100;
    await microtasksFinished();
    markers.forEach(marker => {
      assertTrue(marker.classList.contains('active-marker'));
    });
    crSlider.value = 50;
    await microtasksFinished();
    markers.slice(0, 5).forEach(marker => {
      assertTrue(marker.classList.contains('active-marker'));
    });
    markers.slice(5).forEach(marker => {
      assertTrue(marker.classList.contains('inactive-marker'));
    });
  });

  test('ticks and aria', async () => {
    crSlider.value = 2;
    crSlider.ticks = [1, 2, 4, 8];
    await microtasksFinished();

    assertEquals('1', crSlider.getAttribute('aria-valuemin'));
    assertEquals('8', crSlider.getAttribute('aria-valuemax'));
    assertEquals('4', crSlider.getAttribute('aria-valuetext'));
    assertEquals('4', crSlider.getAttribute('aria-valuenow'));
    assertEquals(
        '', crSlider.shadowRoot!.querySelector('#label')!.textContent!.trim());
    assertEquals(2, crSlider.value);
    pressArrowRight();
    assertEquals(3, crSlider.value);
    await microtasksFinished();
    assertEquals('8', crSlider.getAttribute('aria-valuetext'));
    assertEquals('8', crSlider.getAttribute('aria-valuenow'));
    assertEquals(
        '', crSlider.shadowRoot!.querySelector('#label')!.textContent!.trim());

    crSlider.value = 2;
    crSlider.ticks = [
      {
        value: 10,
        ariaValue: 1,
        label: 'First',
      },
      {
        value: 20,
        label: 'Second',
      },
      {
        value: 30,
        ariaValue: 3,
        label: 'Third',
      },
    ];
    await microtasksFinished();

    assertEquals('1', crSlider.getAttribute('aria-valuemin'));
    assertEquals('3', crSlider.getAttribute('aria-valuemax'));
    assertEquals('Third', crSlider.getAttribute('aria-valuetext'));
    assertEquals(
        'Third',
        crSlider.shadowRoot!.querySelector('#label')!.textContent!.trim());
    assertEquals('3', crSlider.getAttribute('aria-valuenow'));
    pressArrowLeft();
    await microtasksFinished();
    assertEquals('Second', crSlider.getAttribute('aria-valuetext'));
    assertEquals('20', crSlider.getAttribute('aria-valuenow'));
    assertEquals(
        'Second',
        crSlider.shadowRoot!.querySelector('#label')!.textContent!.trim());
  });

  test('disabled whenever public |disabled| is true', async () => {
    crSlider.disabled = true;
    crSlider.ticks = [];
    await microtasksFinished();
    checkDisabled(true);

    crSlider.ticks = [1];
    await microtasksFinished();
    checkDisabled(true);

    crSlider.ticks = [1, 2, 3];
    await microtasksFinished();
    checkDisabled(true);
  });

  test('not disabled or snaps when |ticks| is empty', async () => {
    assertFalse(crSlider.disabled);
    crSlider.ticks = [];
    await microtasksFinished();
    checkDisabled(false);
    assertFalse(crSlider.snaps);
    assertEquals(0, crSlider.min);
    assertEquals(100, crSlider.max);
  });

  test('effectively disabled when only one tick', async () => {
    assertFalse(crSlider.disabled);
    crSlider.ticks = [1];
    await microtasksFinished();
    checkDisabled(true);
    assertFalse(crSlider.snaps);
    assertEquals(0, crSlider.min);
    assertEquals(100, crSlider.max);
  });

  test('not disabled and |snaps| true when |ticks.length| > 0', async () => {
    assertFalse(crSlider.disabled);
    crSlider.ticks = [1, 2, 3];
    await microtasksFinished();
    checkDisabled(false);
    assertTrue(crSlider.snaps);
    assertEquals(0, crSlider.min);
    assertEquals(2, crSlider.max);
  });

  test(
      'disabled, max, min and snaps update when ticks is mutated', async () => {
        assertFalse(crSlider.disabled);
        checkDisabled(false);

        // Single tick is effectively disabled.
        let ticks = crSlider.ticks.slice() as number[];
        ticks.push(1);
        crSlider.ticks = ticks;
        await microtasksFinished();
        checkDisabled(true);
        assertFalse(crSlider.snaps);
        assertEquals(0, crSlider.min);
        assertEquals(100, crSlider.max);

        // Multiple ticks is enabled.
        ticks = crSlider.ticks.slice();
        ticks.push(2);
        crSlider.ticks = ticks;
        await microtasksFinished();
        checkDisabled(false);
        assertTrue(crSlider.snaps);
        assertEquals(0, crSlider.min);
        assertEquals(1, crSlider.max);
      });

  test('value updated before dragging-changed event handled', () => {
    const wait = new Promise<void>(resolve => {
      crSlider.addEventListener('dragging-changed', e => {
        if (!(e as CustomEvent<{value: number}>).detail.value) {
          assertEquals(50, crSlider.value);
          resolve();
        }
      });
    });
    pointerDown(0);
    pointerMove(.5);
    pointerUp();
    return wait;
  });

  test('smooth position transition only on pointerdown', async () => {
    function assertNoTransition() {
      const expected = 'all';
      assertEquals(
          expected,
          getComputedStyle(crSlider.shadowRoot!.querySelector('#knobAndLabel')!)
              .transition);
      assertEquals(
          expected,
          getComputedStyle(crSlider.shadowRoot!.querySelector('#bar')!)
              .transition);
    }

    const knobAndLabel =
        crSlider.shadowRoot!.querySelector<HTMLElement>('#knobAndLabel')!;

    type TransitionEventName = 'transitionstart'|'transitionend';
    function whenTransitionEvent(eventName: TransitionEventName):
        Promise<void> {
      return new Promise(resolve => {
        knobAndLabel.addEventListener(
            eventName, function f(e: TransitionEvent) {
              if (e.target !== knobAndLabel) {
                // Ignore any events coming from the paper-ripple or #label.
                return;
              }

              if (e.propertyName !== 'margin-left') {
                // Ignore all other property transitions, as 'margin-inline-end'
                // (surfaced as 'margin-left') is what actually moves the knob
                // along the horizontal axis.
                return;
              }
              knobAndLabel.removeEventListener(eventName, f);
              resolve();
            });
      });
    }

    const whenTransitionStart = whenTransitionEvent('transitionstart');
    const whenTransitionEnd = whenTransitionEvent('transitionend');

    assertNoTransition();
    pointerDown(.5);
    await Promise.all([whenTransitionStart, whenTransitionEnd]);
    assertNoTransition();

    // Other operations that change the value do not have transitions.
    pointerMove(0);
    await microtasksFinished();
    assertNoTransition();
    assertEquals(0, crSlider.value);
    pointerUp();
    pressArrowRight();
    await microtasksFinished();
    assertNoTransition();
    assertEquals(1, crSlider.value);
    crSlider.value = 50;
    await microtasksFinished();
    assertNoTransition();

    // Check that the slider is not stuck with a transition when the value
    // does not change.
    function unexpectedEventListener(e: TransitionEvent) {
      if (e.target !== knobAndLabel) {
        // Ignore any events coming from the paper-ripple or #label.
        return;
      }
      knobAndLabel.removeEventListener(
          e.type as TransitionEventName, unexpectedEventListener);
      assertNotReached(`Unexpected '${e.type}' event for '${e.propertyName}'`);
    }
    knobAndLabel.addEventListener('transitionstart', unexpectedEventListener);
    knobAndLabel.addEventListener('transitionend', unexpectedEventListener);

    // Need to yield here, otherwise for some unknown odd reason the events that
    // would cause this test to fail above are not fired, when they should.
    await new Promise<void>(resolve => window.setTimeout(() => resolve(), 1));

    crSlider.value = 0;
    await microtasksFinished();
    pointerDown(0);

    // Wait a bit to allow for any undesired events to surface.
    await new Promise<void>(resolve => window.setTimeout(() => resolve(), 150));
  });

  test('getRatio()', () => {
    crSlider.min = 1;
    crSlider.max = 11;
    crSlider.value = 1;
    assertEquals(0, crSlider.getRatio());
    crSlider.value = 11;
    assertEquals(1, crSlider.getRatio());
    crSlider.value = 6;
    assertEquals(.5, crSlider.getRatio());
  });

  test('cr-slider-value-changed event when mouse clicked', async () => {
    assertEquals(0, crSlider.value);
    const whenFired = eventToPromise('cr-slider-value-changed', crSlider);
    pointerDown(.1);
    await whenFired;
    assertEquals(10, crSlider.value);
  });

  test('cr-slider-value-changed event when key pressed', async () => {
    assertEquals(0, crSlider.value);
    const whenFired = eventToPromise('cr-slider-value-changed', crSlider);
    pressArrowRight();
    await whenFired;
    assertEquals(1, crSlider.value);
  });

  test(
      'out of range value updated back into min/max range with debounce',
      async () => {
        crSlider.min = -100;
        crSlider.max = 1000;
        crSlider.value = -50;
        await microtasksFinished();
        assertEquals(-50, crSlider.value);

        crSlider.min = 0;
        crSlider.max = 100;
        crSlider.value = 150;
        // Clamping value should happen async, not sync, in order to not race
        // when min/max and value change at the same time.
        assertEquals(150, crSlider.value);

        await microtasksFinished();
        assertEquals(100, crSlider.value);

        crSlider.max = 25;
        await microtasksFinished();
        assertEquals(25, crSlider.value);

        crSlider.min = 50;
        crSlider.max = 100;
        await microtasksFinished();
        assertEquals(50, crSlider.value);
      });

  test('InitialDefaultValue', () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    crSlider = document.createElement('cr-slider');
    document.body.appendChild(crSlider);

    assertEquals(0, crSlider.value);
    assertTrue(isVisible(crSlider.$.container));
  });
});
