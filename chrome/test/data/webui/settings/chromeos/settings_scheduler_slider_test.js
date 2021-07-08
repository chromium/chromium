// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {keyDownOn, keyUpOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// clang-format on

/** @fileoverview Suite of tests for settings-scheduler-slider. */
suite('SettingsSchedulerSlider', function() {
  /** @type {!SettingsSchedulerSliderElement} */
  let slider;

  setup(function() {
    PolymerTest.clearBody();
    slider = document.createElement('settings-scheduler-slider');
    slider.prefStartTime = {
      key: 'ash.night_light.custom_start_time',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 60,
    };
    slider.prefEndTime = {
      key: 'ash.night_light.custom_end_time',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 120,
    };
    assertTrue(!!slider);
    slider.prefs = {
      ash: {
        night_light: {
          custom_start_time: {
            key: 'ash.night_light.custom_start_time',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 60,
          },
          custom_end_time: {
            key: 'ash.night_light.custom_start_time',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 120,
          },
        }
      },
      settings: {
        clock: {
          use_24hour_clock: {
            key: 'settings.clock.use_24hour_clock',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
      }
    };
    document.body.appendChild(slider);
  });

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('pref value update time string', async function() {
    // Test that the slider time string is updated after the pref is
    // saved.
    assertTrue(!!slider.$$('#startLabel'));
    assertTrue(!!slider.$$('#endLabel'));

    const getStartTimeString = () => {
      return slider.$$('#startLabel').innerHTML.trim();
    };

    const getEndTimeString = () => {
      return slider.$$('#endLabel').innerHTML.trim();
    };

    assertEquals('1:00 AM', getStartTimeString());
    assertEquals('2:00 AM', getEndTimeString());

    slider.prefStartTime.value = 70;
    slider.setPrefValue(
        'ash.night_light.custom_start_time', slider.prefStartTime.value);
    await flushAsync();

    assertEquals('1:10 AM', getStartTimeString());
    assertEquals('2:00 AM', getEndTimeString());

    slider.prefEndTime.value = 900;
    slider.setPrefValue(
        'ash.night_light.custom_end_time', slider.prefEndTime.value);
    await flushAsync();
    assertEquals('1:10 AM', getStartTimeString());
    assertEquals('3:00 PM', getEndTimeString());
  });
});
