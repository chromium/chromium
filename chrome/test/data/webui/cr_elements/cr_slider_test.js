// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-slider', function() {
  let crSlider;

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = '<cr-slider min="0" max="100"></cr-slider>';

    crSlider = document.body.querySelector('cr-slider');
  });

  /** @param {boolean} expected */
  function checkDisabled(expected) {
    assertEquals(
        expected,
        window.getComputedStyle(crSlider)['pointer-events'] == 'none');
  }

  function pressArrowRight() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 39, [], 'ArrowRight');
  }

  function pressArrowLeft() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 37, [], 'ArrowLeft');
  }

  function pressPageUp() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 33, [], 'PageUp');
  }

  function pressPageDown() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 34, [], 'PageDown');
  }

  function pressArrowUp() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 38, [], 'ArrowUp');
  }

  function pressArrowDown() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 40, [], 'ArrowDown');
  }

  function pressHome() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 36, [], 'Home');
  }

  function pressEnd() {
    MockInteractions.pressAndReleaseKeyOn(crSlider, 35, [], 'End');
  }

  function pointerEvent(eventType, ratio) {
    const rect = crSlider.$.barContainer.getBoundingClientRect();
    crSlider.dispatchEvent(new PointerEvent(eventType, {
      buttons: 1,
      pointerId: 1,
      clientX: rect.left + (ratio * rect.width),
    }));
  }

  function pointerDown(ratio) {
    pointerEvent('pointerdown', ratio);
  }

  function pointerMove(ratio) {
    pointerEvent('pointermove', ratio);
  }

  function pointerUp() {
    // Ignores clientX for pointerup event.
    pointerEvent('pointerup', 0);
  }

  // Ensure that value-changed event bubbles, since users of cr-slider rely on
  // such event.
  test('value-changed bubbles', function() {
    const whenFired = test_util.eventToPromise('value-changed', crSlider);
    crSlider.value = 50;
    return whenFired;
  });

  test('key events', () => {
    crSlider.value = 0;
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

  test('mouse events', () => {
    crSlider.value = 0;
    pointerMove(.25);
    assertEquals(0, crSlider.value);
    pointerDown(.5);
    assertEquals(50, crSlider.value);
    assertEquals(5, crSlider.draggingEventTracker_.listeners_.length);
    pointerMove(.75);
    assertEquals(75, crSlider.value);
    pointerMove(-1);
    assertEquals(0, crSlider.value);
    pointerMove(2);
    assertEquals(100, crSlider.value);
    pointerUp();
    assertEquals(100, crSlider.value);
    assertEquals(0, crSlider.draggingEventTracker_.listeners_.length);
    pointerMove(.25);
    assertEquals(100, crSlider.value);
  });

  test('update value instantly both off and on', () => {
    crSlider.updateValueInstantly = false;
    assertEquals(0, crSlider.value);
    pointerDown(.5);
    assertEquals(0, crSlider.value);
    pointerUp();
    assertEquals(50, crSlider.value);

    // Once |updateValueInstantly| is turned on, |value| should start updating
    // again during drag.
    pointerDown(0);
    assertEquals(50, crSlider.value);
    crSlider.updateValueInstantly = true;
    pointerMove(0);
    assertEquals(0, crSlider.value);
    crSlider.updateValueInstantly = false;
    pointerMove(.4);
    assertEquals(0, crSlider.value);
    pointerUp();
    assertEquals(40, crSlider.value);
  });

  test('snaps to closest value', () => {
    crSlider.snaps = true;
    pointerDown(.501);
    assertEquals(50, crSlider.value);
    pointerMove(.505);
    assertEquals(51, crSlider.value);
  });

  test('markers', () => {
    crSlider.value = 0;
    assertTrue(crSlider.$.markers.hidden);
    crSlider.markerCount = 10;
    assertFalse(crSlider.$.markers.hidden);
    Polymer.dom.flush();
    const markers = Array.from(crSlider.root.querySelectorAll('#markers div'));
    assertEquals(9, markers.length);
    markers.forEach((marker, i) => {
      assertTrue(marker.classList.contains('inactive-marker'));
    });
    crSlider.value = 100;
    markers.forEach(marker => {
      assertTrue(marker.classList.contains('active-marker'));
    });
    crSlider.value = 50;
    markers.slice(0, 5).forEach(marker => {
      assertTrue(marker.classList.contains('active-marker'));
    });
    markers.slice(5).forEach(marker => {
      assertTrue(marker.classList.contains('inactive-marker'));
    });
  });

  test('ticks and aria', () => {
    crSlider.value = 1.5;
    crSlider.ticks = [1, 2, 4, 8];
    assertEquals('1', crSlider.getAttribute('aria-valuemin'));
    assertEquals('8', crSlider.getAttribute('aria-valuemax'));
    assertEquals('4', crSlider.getAttribute('aria-valuetext'));
    assertEquals('4', crSlider.getAttribute('aria-valuenow'));
    assertEquals('', crSlider.$.label.innerHTML.trim());
    assertEquals(2, crSlider.value);
    crSlider.value = 100;
    assertEquals(3, crSlider.value);
    assertEquals('8', crSlider.getAttribute('aria-valuetext'));
    assertEquals('8', crSlider.getAttribute('aria-valuenow'));
    assertEquals('', crSlider.$.label.innerHTML.trim());
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
    assertEquals('1', crSlider.getAttribute('aria-valuemin'));
    assertEquals('3', crSlider.getAttribute('aria-valuemax'));
    assertEquals('Third', crSlider.getAttribute('aria-valuetext'));
    assertEquals('Third', crSlider.$.label.innerHTML.trim());
    assertEquals('3', crSlider.getAttribute('aria-valuenow'));
    crSlider.value = 1;
    assertEquals('Second', crSlider.getAttribute('aria-valuetext'));
    assertEquals('20', crSlider.getAttribute('aria-valuenow'));
    assertEquals('Second', crSlider.$.label.innerHTML.trim());
  });

  test('disabled whenever public |disabled| is true', () => {
    crSlider.disabled = true;
    crSlider.ticks = [];
    checkDisabled(true);
    crSlider.ticks = [1];
    checkDisabled(true);
    crSlider.ticks = [1, 2, 3];
    checkDisabled(true);
  });

  test('not disabled or snaps when |ticks| is empty', () => {
    assertFalse(crSlider.disabled);
    crSlider.ticks = [];
    checkDisabled(false);
    assertFalse(crSlider.snaps);
    assertEquals(0, crSlider.min);
    assertEquals(100, crSlider.max);
  });

  test('effectively disabled when only one tick', () => {
    assertFalse(crSlider.disabled);
    crSlider.ticks = [1];
    checkDisabled(true);
    assertFalse(crSlider.snaps);
    assertEquals(0, crSlider.min);
    assertEquals(100, crSlider.max);
  });

  test('not disabled and |snaps| true when |ticks.length| > 0', () => {
    assertFalse(crSlider.disabled);
    crSlider.ticks = [1, 2, 3];
    checkDisabled(false);
    assertTrue(crSlider.snaps);
    assertEquals(0, crSlider.min);
    assertEquals(2, crSlider.max);
  });

  test('disabled, max, min and snaps update when ticks is mutated', () => {
    assertFalse(crSlider.disabled);
    checkDisabled(false);

    // Single tick is effectively disabled.
    crSlider.push('ticks', 1);
    checkDisabled(true);
    assertFalse(crSlider.snaps);
    assertEquals(0, crSlider.min);
    assertEquals(100, crSlider.max);

    // Multiple ticks is enabled.
    crSlider.push('ticks', 2);
    checkDisabled(false);
    assertTrue(crSlider.snaps);
    assertEquals(0, crSlider.min);
    assertEquals(1, crSlider.max);
  });
});
