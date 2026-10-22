// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {keyDownOn, keyUpOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {CrSliderElement,SettingsSliderElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
// clang-format on

/** @fileoverview Suite of tests for settings-slider. */
suite('SettingsSlider', function() {
  let slider: SettingsSliderElement;
  let crSlider: CrSliderElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  const ticks: number[] = [2, 4, 8, 16, 32, 64, 128];

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const initialPrefs = [
      {
        key: 'test_pref',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 16,
      },
      {
        key: 'other_pref',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 32,
      },
    ];

    prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);

    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();

    slider = document.createElement('settings-slider');
    slider.prefKey = 'test_pref';

    document.body.appendChild(slider);
    crSlider = slider.shadowRoot.querySelector('cr-slider')!;
    return microtasksFinished();
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
    prefsBrowserProxy.fakeApi.sendPrefChanges([
      {key: 'test_pref', value: prefValue},
    ]);
    await microtasksFinished();
    assertEquals(sliderValue, crSlider.value);
  }

  test('enforce value', async function() {
    // Test that the indicator is not present until after the pref is
    // enforced.
    let indicator = slider.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertFalse(!!indicator);
    prefsBrowserProxy.fakeApi.prefs['test_pref'] = {
      key: 'test_pref',
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 16,
    };
    prefsBrowserProxy.fakeApi.sendPrefChanges([
      {key: 'test_pref', value: 16},
    ]);
    await microtasksFinished();
    indicator = slider.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertTrue(!!indicator);
  });

  test('set value', async () => {
    slider.ticks = ticks;
    await checkSliderValueFromPref(8, 2);
    assertEquals(6, crSlider.max);

    // settings-slider only supports snapping to a range of tick values.
    // Setting to an in-between value should snap to an indexed value.
    await checkSliderValueFromPref(70, 5);
    assertEquals(64, prefService.getPref<number>('test_pref').value);

    // Setting the value out-of-range should clamp the slider.
    await checkSliderValueFromPref(-100, 0);
    assertEquals(2, prefService.getPref<number>('test_pref').value);
  });

  test('move slider', async () => {
    slider.ticks = ticks;
    await checkSliderValueFromPref(30, 4);

    await pressArrowRight();
    assertEquals(5, crSlider.value);
    assertEquals(64, prefService.getPref<number>('test_pref').value);

    await pressArrowRight();
    assertEquals(6, crSlider.value);
    assertEquals(128, prefService.getPref<number>('test_pref').value);

    await pressArrowRight();
    assertEquals(6, crSlider.value);
    assertEquals(128, prefService.getPref<number>('test_pref').value);

    await pressArrowLeft();
    assertEquals(5, crSlider.value);
    assertEquals(64, prefService.getPref<number>('test_pref').value);

    await pressPageUp();
    assertEquals(6, crSlider.value);
    assertEquals(128, prefService.getPref<number>('test_pref').value);

    await pressPageDown();
    assertEquals(5, crSlider.value);
    assertEquals(64, prefService.getPref<number>('test_pref').value);

    await pressHome();
    assertEquals(0, crSlider.value);
    assertEquals(2, prefService.getPref<number>('test_pref').value);

    await pressArrowDown();
    assertEquals(0, crSlider.value);
    assertEquals(2, prefService.getPref<number>('test_pref').value);

    await pressArrowUp();
    assertEquals(1, crSlider.value);
    assertEquals(4, prefService.getPref<number>('test_pref').value);

    await pressEnd();
    assertEquals(6, crSlider.value);
    assertEquals(128, prefService.getPref<number>('test_pref').value);
  });

  test('scaled slider', async () => {
    await checkSliderValueFromPref(2, 2);

    slider.scale = 10;
    slider.max = 4;

    await pressArrowRight();
    assertEquals(3, crSlider.value);
    assertEquals(.3, prefService.getPref<number>('test_pref').value);

    await pressArrowRight();
    assertEquals(4, crSlider.value);
    assertEquals(.4, prefService.getPref<number>('test_pref').value);

    await pressArrowRight();
    assertEquals(4, crSlider.value);
    assertEquals(.4, prefService.getPref<number>('test_pref').value);

    await pressHome();
    assertEquals(0, crSlider.value);
    assertEquals(0, prefService.getPref<number>('test_pref').value);

    await pressEnd();
    assertEquals(4, crSlider.value);
    assertEquals(.4, prefService.getPref<number>('test_pref').value);

    await checkSliderValueFromPref(.25, 2.5);
    assertEquals(.25, prefService.getPref<number>('test_pref').value);

    await pressPageUp();
    assertEquals(3.5, crSlider.value);
    assertEquals(.35, prefService.getPref<number>('test_pref').value);

    await pressPageUp();
    assertEquals(4, crSlider.value);
    assertEquals(.4, prefService.getPref<number>('test_pref').value);
  });

  test('update value instantly both off and on with ticks', async () => {
    slider.ticks = ticks;
    await checkSliderValueFromPref(4, 1);
    slider.updateValueInstantly = false;
    pointerDown(3 / crSlider.max);
    assertEquals(3, crSlider.value);
    assertEquals(4, prefService.getPref<number>('test_pref').value);
    pointerUp();
    await eventToPromise('dragging-changed', crSlider);
    assertEquals(3, crSlider.value);
    assertEquals(16, prefService.getPref<number>('test_pref').value);

    // Once |updateValueInstantly| is turned on, |value| should start updating
    // again during drag.
    pointerDown(0);
    assertEquals(0, crSlider.value);
    assertEquals(16, prefService.getPref<number>('test_pref').value);
    slider.updateValueInstantly = true;
    await microtasksFinished();
    assertEquals(2, prefService.getPref<number>('test_pref').value);
    pointerMove(1 / crSlider.max);
    assertEquals(1, crSlider.value);
    assertEquals(4, prefService.getPref<number>('test_pref').value);
    slider.updateValueInstantly = false;
    pointerMove(2 / crSlider.max);
    assertEquals(2, crSlider.value);
    assertEquals(4, prefService.getPref<number>('test_pref').value);
    pointerUp();
    await eventToPromise('dragging-changed', crSlider);
    assertEquals(2, crSlider.value);
    assertEquals(8, prefService.getPref<number>('test_pref').value);
  });

  test('update value instantly both off and on', async () => {
    slider.scale = 10;
    await checkSliderValueFromPref(2, 20);
    slider.updateValueInstantly = false;
    pointerDown(.3);
    assertCloseTo(30, crSlider.value);
    assertEquals(2, prefService.getPref<number>('test_pref').value);
    pointerUp();
    await eventToPromise('dragging-changed', crSlider);
    assertCloseTo(30, crSlider.value);
    assertCloseTo(3, prefService.getPref<number>('test_pref').value);

    // Once |updateValueInstantly| is turned on, |value| should start updating
    // again during drag.
    pointerDown(0);
    assertEquals(0, crSlider.value);
    assertCloseTo(3, prefService.getPref<number>('test_pref').value);
    slider.updateValueInstantly = true;
    await microtasksFinished();
    assertEquals(0, prefService.getPref<number>('test_pref').value);
    pointerMove(.1);
    assertCloseTo(10, crSlider.value);
    assertCloseTo(1, prefService.getPref<number>('test_pref').value);
    slider.updateValueInstantly = false;
    pointerMove(.2);
    assertCloseTo(20, crSlider.value);
    assertCloseTo(1, prefService.getPref<number>('test_pref').value);
    pointerUp();
    await eventToPromise('dragging-changed', crSlider);
    assertCloseTo(20, crSlider.value);
    assertCloseTo(2, prefService.getPref<number>('test_pref').value);
  });


  test('value updated while updatingFromKey is true', async () => {
    slider.ticks = ticks;
    await checkSliderValueFromPref(4, 1);

    keyDownOn(crSlider, 0, [], 'ArrowRight');
    assertTrue(crSlider.updatingFromKey);
    assertEquals(2, crSlider.value);
    assertEquals(8, prefService.getPref<number>('test_pref').value);

    // Simulate an external pref change arriving while key is held down.
    prefsBrowserProxy.fakeApi.sendPrefChanges([
      {key: 'test_pref', value: 4},
    ]);
    await microtasksFinished();
    assertEquals(2, crSlider.value);

    keyUpOn(crSlider, 0, [], 'ArrowRight');
    await microtasksFinished();
    assertFalse(crSlider.updatingFromKey);
    assertEquals(8, prefService.getPref<number>('test_pref').value);
  });
});
