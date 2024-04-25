// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// clang-format off

import 'chrome://os-settings/os_settings.js';

import {keyDownOn, keyUpOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrSliderElement,SettingsSliderV2Element} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../../utils.js';

/** @fileoverview Suite of tests for settings-slider-v2. */
suite('SettingsSliderV2', () => {
  let slider: SettingsSliderV2Element;

  /**
   * cr-slider instance wrapped by settings-slider-v2.
   */
  let internalSlider: CrSliderElement;

  const ticks: number[] = [2, 4, 8, 16, 32, 64, 128];

  setup(async () => {
    clearBody();
    slider = document.createElement('settings-slider-v2');
    document.body.appendChild(slider);
    internalSlider = slider.shadowRoot!.querySelector('cr-slider')!;
    await flushTasks();
  });

  function press(key: string) {
    keyDownOn(internalSlider, 0, [], key);
    keyUpOn(internalSlider, 0, [], key);
  }

  function pointerEvent(eventType: string, ratio: number) {
    const rect = internalSlider.shadowRoot!.querySelector<HTMLElement>(
                                         '#container')!.getBoundingClientRect();
    internalSlider.dispatchEvent(new PointerEvent(eventType, {
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
    assertNotEquals(sliderValue, internalSlider.value);
    if (internalSlider.updatingFromKey) {
      await eventToPromise('updating-from-key-changed', internalSlider);
    }
    slider.set('pref.value', prefValue);
    assertEquals(sliderValue, internalSlider.value);
  }

  suite('with pref specified', () => {
    const fakePrefObject = {
      key: 'testPref',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 16,
    };

    setup(async () => {
      slider.pref = {...fakePrefObject};
      await flushTasks();
    });

    test('disabled slider if pref is enforced', () => {
      // Test that the slider is disabled even manually set disabled to false if
      // the pref is enforced.
      assertFalse(slider.disabled);

      slider.pref = {
        ...fakePrefObject,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      slider.disabled = false;

      flush();
      assertTrue(slider.disabled);
      assertEquals('true', internalSlider.ariaDisabled);
    });

    test('disabled slider if ticks has one value', () => {
      // Test that the slider is disabled even manually set disabled to false if
      // ticks has one value.
      assertFalse(slider.disabled);
      slider.disabled = false;
      slider.ticks = [2];

      flush();
      assertTrue(slider.disabled);
      assertEquals('true', internalSlider.ariaDisabled);
    });

    test('indicator is not present until after the pref is enforced', () => {
      let indicator =
          slider.shadowRoot!.querySelector('cr-policy-pref-indicator');
      assertFalse(isVisible(indicator));
      slider.pref = {
        ...fakePrefObject,
        controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
      flush();
      indicator = slider.shadowRoot!.querySelector('cr-policy-pref-indicator');
      assertTrue(isVisible(indicator));
    });

    [true, false].forEach(hideLabel => {
      test('visibility of labels', () => {
        slider.hideLabel = hideLabel;
        flush();

        const labels = slider.shadowRoot!.querySelector<HTMLElement>('#labels');
        assertTrue(!!labels);

        assertEquals(hideLabel, labels.hidden);
      });
    });

    test('should focus the internal slider', () => {
      assertNotEquals(
          internalSlider, slider.shadowRoot!.activeElement);
      slider.focus();
      assertEquals(
          internalSlider, slider.shadowRoot!.activeElement);
    });

    test('move slider dispatches pref value change event', async () => {
      slider.ticks = ticks;
      await checkSliderValueFromPref(/*prefValue=*/ 30, /*sliderValue=*/ 4);

      const prefChangeEventPromise = eventToPromise('user-action-setting-pref-change', window);
      // Drag the knob on slider to the right. The next value on the right should be 64.
      press('ArrowRight');
      const newValue = 64;
      assertEquals(newValue, slider.pref?.value);

      const event = await prefChangeEventPromise;
      assertEquals(fakePrefObject.key, event.detail.prefKey);
      assertEquals(newValue, event.detail.value);
    });

    test('slider value updates from pref value', async () => {
      slider.ticks = ticks;
      await checkSliderValueFromPref(/*prefValue=*/ 8, /*sliderValue=*/ 2);
      assertEquals(6, internalSlider.max);

      // settings-slider-v2 only supports snapping to a range of tick values.
      // Setting to an in-between value should snap to an indexed value.
      await checkSliderValueFromPref(/*prefValue=*/ 70, /*sliderValue=*/ 5);
      assertEquals(64, slider.pref?.value);

      // Setting the value out-of-range should clamp the slider.
      await checkSliderValueFromPref(/*prefValue=*/ -100, /*sliderValue=*/ 0);
      assertEquals(2, slider.pref?.value);
    });

    test('move slider via keypress', async () => {
      slider.ticks = ticks;
      await checkSliderValueFromPref(/*prefValue=*/ 30, /*sliderValue=*/ 4);

      press('ArrowRight');
      assertEquals(5, internalSlider.value);
      assertEquals(64, slider.pref?.value);

      press('ArrowRight');
      assertEquals(6, internalSlider.value);
      assertEquals(128, slider.pref?.value);

      press('ArrowRight');
      assertEquals(6, internalSlider.value);
      assertEquals(128, slider.pref?.value);

      press('ArrowLeft');
      assertEquals(5, internalSlider.value);
      assertEquals(64, slider.pref?.value);

      press('PageUp');
      assertEquals(6, internalSlider.value);
      assertEquals(128, slider.pref?.value);

      press('PageDown');
      assertEquals(5, internalSlider.value);
      assertEquals(64, slider.pref?.value);

      press('Home');
      assertEquals(0, internalSlider.value);
      assertEquals(2, slider.pref?.value);

      press('ArrowDown');
      assertEquals(0, internalSlider.value);
      assertEquals(2, slider.pref?.value);

      press('ArrowUp');
      assertEquals(1, internalSlider.value);
      assertEquals(4, slider.pref?.value);

      press('End');
      assertEquals(6, internalSlider.value);
      assertEquals(128, slider.pref?.value);
    });

    test('scaled slider', async () => {
      await checkSliderValueFromPref(/*prefValue=*/2, /*sliderValue=*/ 2);

      slider.scale = 10;
      slider.max = 4;
      press('ArrowRight');
      assertEquals(3, internalSlider.value);
      assertEquals(.3, slider.pref?.value);

      press('ArrowRight');
      assertEquals(4, internalSlider.value);
      assertEquals(.4, slider.pref?.value);

      press('ArrowRight');
      assertEquals(4, internalSlider.value);
      assertEquals(.4, slider.pref?.value);

      press('Home');
      assertEquals(0, internalSlider.value);
      assertEquals(0, slider.pref?.value);

      press('End');
      assertEquals(4, internalSlider.value);
      assertEquals(.4, slider.pref?.value);

      await checkSliderValueFromPref(/*prefValue=*/ .25, /*sliderValue=*/ 2.5);
      assertEquals(.25, slider.pref?.value);

      press('PageUp');
      assertEquals(3.5, internalSlider.value);
      assertEquals(.35, slider.pref?.value);

      press('PageUp');
      assertEquals(4, internalSlider.value);
      assertEquals(.4, slider.pref?.value);
    });

    test('value updates instantly with ticks', async () => {
      slider.ticks = ticks;
      slider.updateValueInstantly = true;
      await checkSliderValueFromPref(/*prefValue=*/ 4, /*sliderValue=*/ 1);

      pointerDown(0);
      pointerMove(3 / internalSlider.max);
      assertEquals(3, internalSlider.value);
      assertEquals(16, slider.pref?.value);
    });

    test('value updates after drag is done with ticks', async () => {
      slider.ticks = ticks;
      slider.updateValueInstantly = false;
      await checkSliderValueFromPref(/*prefValue=*/ 4, /*sliderValue=*/ 1);

      pointerDown(3 / internalSlider.max);
      assertEquals(3, internalSlider.value);
      assertEquals(4, slider.pref?.value);
      pointerUp();
      // Pref value updates when dragging is finishend.
      assertEquals(3, internalSlider.value);
      assertEquals(16, slider.pref?.value);
    });

    test('value updates instantly with scale', async () => {
      slider.scale = 10;
      slider.updateValueInstantly = true;
      await checkSliderValueFromPref(/*prefValue=*/ 2, /*sliderValue=*/ 20);

      pointerDown(0);
      pointerDown(.3);
      assertCloseTo(30, internalSlider.value);
      assertCloseTo(3, slider.pref?.value);
    });

    test('value updates after drag is done with scale', async () => {
      slider.scale = 10;
      slider.updateValueInstantly = false;
      await checkSliderValueFromPref(/*prefValue=*/ 2, /*sliderValue=*/ 20);

      pointerDown(.3);
      assertCloseTo(30, internalSlider.value);
      assertEquals(2, slider.pref?.value);
      pointerUp();
      // Pref value updates when dragging is finishend.
      assertCloseTo(30, internalSlider.value);
      assertCloseTo(3, slider.pref?.value);
    });
  });

  // TODO(b/333454006): Add tests for not pref set.
});
