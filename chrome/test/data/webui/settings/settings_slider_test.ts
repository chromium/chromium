// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {keyDownOn, keyUpOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrSliderElement,SettingsSliderElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
// clang-format on

/** @fileoverview Suite of tests for settings-slider. */
suite('SettingsSlider', function() {
  let slider: SettingsSliderElement;

  /**
   * cr-slider instance wrapped by settings-slider.
   */
  let crSlider: CrSliderElement;

  const ticks: number[] = [2, 4, 8, 16, 32, 64, 128];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    slider = document.createElement('settings-slider');
    slider.pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 16,
    };
    document.body.appendChild(slider);
    crSlider = slider.shadowRoot!.querySelector('cr-slider')!;
    return flushTasks();
  });

  function press(key: string) {
    keyDownOn(crSlider, 0, [], key);
    keyUpOn(crSlider, 0, [], key);
    return microtasksFinished();
  }

  function pressArrowRight() {
    return press('ArrowRight');
  }

  function pressArrowLeft() {
    return press('ArrowLeft');
  }

  function pressPageUp() {
    return press('PageUp');
  }

  function pressPageDown() {
    return press('PageDown');
  }

  function pressArrowUp() {
    return press('ArrowUp');
  }

  function pressArrowDown() {
    return press('ArrowDown');
  }

  function pressHome() {
    return press('Home');
  }

  function pressEnd() {
    return press('End');
  }

  function pointerEvent(eventType: string, ratio: number) {
    const rect = crSlider.shadowRoot!.querySelector<HTMLElement>(
                                         '#container')!.getBoundingClientRect();
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

  function assertCloseTo(actual: number, expected: number) {
    assertTrue(
        Math.abs(1 - actual / expected) <= Number.EPSILON,
        `expected ${expected} to be close to ${actual}`);
  }

  async function checkSliderValueFromPref(
      prefValue: number, sliderValue: number) {
    assertNotEquals(sliderValue, crSlider.value);
    if (crSlider.updatingFromKey) {
      await eventToPromise('updating-from-key-changed', crSlider);
    }
    slider.set('pref.value', prefValue);
    assertEquals(sliderValue, crSlider.value);
  }

  test('enforce value', function() {
    // Test that the indicator is not present until after the pref is
    // enforced.
    let indicator =
        slider.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertFalse(!!indicator);
    slider.pref = {
      key: '',
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 16,
    };
    flush();
    indicator = slider.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertTrue(!!indicator);
  });

  test('set value', async () => {
    slider.ticks = ticks;
    await checkSliderValueFromPref(8, 2);
    assertEquals(6, crSlider.max);

    // settings-slider only supports snapping to a range of tick values.
    // Setting to an in-between value should snap to an indexed value.
    await checkSliderValueFromPref(70, 5);
    assertEquals(64, slider.pref.value);

    // Setting the value out-of-range should clamp the slider.
    await checkSliderValueFromPref(-100, 0);
    assertEquals(2, slider.pref.value);
  });

  test('move slider', async () => {
    slider.ticks = ticks;
    await checkSliderValueFromPref(30, 4);

    await pressArrowRight();
    assertEquals(5, crSlider.value);
    assertEquals(64, slider.pref.value);

    await pressArrowRight();
    assertEquals(6, crSlider.value);
    assertEquals(128, slider.pref.value);

    await pressArrowRight();
    assertEquals(6, crSlider.value);
    assertEquals(128, slider.pref.value);

    await pressArrowLeft();
    assertEquals(5, crSlider.value);
    assertEquals(64, slider.pref.value);

    await pressPageUp();
    assertEquals(6, crSlider.value);
    assertEquals(128, slider.pref.value);

    await pressPageDown();
    assertEquals(5, crSlider.value);
    assertEquals(64, slider.pref.value);

    await pressHome();
    assertEquals(0, crSlider.value);
    assertEquals(2, slider.pref.value);

    await pressArrowDown();
    assertEquals(0, crSlider.value);
    assertEquals(2, slider.pref.value);

    await pressArrowUp();
    assertEquals(1, crSlider.value);
    assertEquals(4, slider.pref.value);

    await pressEnd();
    assertEquals(6, crSlider.value);
    assertEquals(128, slider.pref.value);
  });

  test('scaled slider', async () => {
    await checkSliderValueFromPref(2, 2);

    slider.scale = 10;
    slider.max = 4;

    await pressArrowRight();
    assertEquals(3, crSlider.value);
    assertEquals(.3, slider.pref.value);

    await pressArrowRight();
    assertEquals(4, crSlider.value);
    assertEquals(.4, slider.pref.value);

    await pressArrowRight();
    assertEquals(4, crSlider.value);
    assertEquals(.4, slider.pref.value);

    await pressHome();
    assertEquals(0, crSlider.value);
    assertEquals(0, slider.pref.value);

    await pressEnd();
    assertEquals(4, crSlider.value);
    assertEquals(.4, slider.pref.value);

    await checkSliderValueFromPref(.25, 2.5);
    assertEquals(.25, slider.pref.value);

    await pressPageUp();
    assertEquals(3.5, crSlider.value);
    assertEquals(.35, slider.pref.value);

    await pressPageUp();
    assertEquals(4, crSlider.value);
    assertEquals(.4, slider.pref.value);
  });

  test('update value instantly both off and on with ticks', async () => {
    slider.ticks = ticks;
    await checkSliderValueFromPref(4, 1);
    slider.updateValueInstantly = false;
    pointerDown(3 / crSlider.max);
    assertEquals(3, crSlider.value);
    assertEquals(4, slider.pref.value);
    pointerUp();
    await eventToPromise('dragging-changed', crSlider);
    assertEquals(3, crSlider.value);
    assertEquals(16, slider.pref.value);

    // Once |updateValueInstantly| is turned on, |value| should start updating
    // again during drag.
    pointerDown(0);
    assertEquals(0, crSlider.value);
    assertEquals(16, slider.pref.value);
    slider.updateValueInstantly = true;
    assertEquals(2, slider.pref.value);
    pointerMove(1 / crSlider.max);
    assertEquals(1, crSlider.value);
    assertEquals(4, slider.pref.value);
    slider.updateValueInstantly = false;
    pointerMove(2 / crSlider.max);
    assertEquals(2, crSlider.value);
    assertEquals(4, slider.pref.value);
    pointerUp();
    await eventToPromise('dragging-changed', crSlider);
    assertEquals(2, crSlider.value);
    assertEquals(8, slider.pref.value);
  });

  test('update value instantly both off and on', async () => {
    slider.scale = 10;
    await checkSliderValueFromPref(2, 20);
    slider.updateValueInstantly = false;
    pointerDown(.3);
    assertCloseTo(30, crSlider.value);
    assertEquals(2, slider.pref.value);
    pointerUp();
    await eventToPromise('dragging-changed', crSlider);
    assertCloseTo(30, crSlider.value);
    assertCloseTo(3, slider.pref.value);

    // Once |updateValueInstantly| is turned on, |value| should start updating
    // again during drag.
    pointerDown(0);
    assertEquals(0, crSlider.value);
    assertCloseTo(3, slider.pref.value);
    slider.updateValueInstantly = true;
    assertEquals(0, slider.pref.value);
    pointerMove(.1);
    assertCloseTo(10, crSlider.value);
    assertCloseTo(1, slider.pref.value);
    slider.updateValueInstantly = false;
    pointerMove(.2);
    assertCloseTo(20, crSlider.value);
    assertCloseTo(1, slider.pref.value);
    pointerUp();
    await eventToPromise('dragging-changed', crSlider);
    assertCloseTo(20, crSlider.value);
    assertCloseTo(2, slider.pref.value);
  });
});
