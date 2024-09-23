// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SettingsSchedulerSliderElement} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<settings-scheduler-slider>', () => {
  let slider: SettingsSchedulerSliderElement;

  setup(() => {
    slider = document.createElement('settings-scheduler-slider');
    assertTrue(!!slider);
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
        },
      },
      settings: {
        clock: {
          use_24hour_clock: {
            key: 'settings.clock.use_24hour_clock',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
      },
    };
    document.body.appendChild(slider);
    flush();
  });

  // TODO(crbug.com/1305868O): Skip test as it consistently fails whenever
  // daylight savings is active.
  test.skip('pref value update time string', () => {
    // Test that the slider time string is updated after the pref is
    // saved.
    const startLabel = slider.shadowRoot!.querySelector('#startLabel');
    assertTrue(!!startLabel);

    const endLabel = slider.shadowRoot!.querySelector('#endLabel');
    assertTrue(!!endLabel);

    const getStartTimeString = () => {
      return startLabel.innerHTML.trim();
    };

    const getEndTimeString = () => {
      return endLabel.innerHTML.trim();
    };

    assertEquals('1:00 AM', getStartTimeString());
    assertEquals('2:00 AM', getEndTimeString());

    slider.prefStartTime.value = 70;
    slider.setPrefValue('ash.night_light.custom_start_time', 70);
    flush();

    assertEquals('1:10 AM', getStartTimeString());
    assertEquals('2:00 AM', getEndTimeString());

    slider.prefEndTime.value = 900;
    slider.setPrefValue(
        'ash.night_light.custom_end_time', slider.prefEndTime.value);
    flush();
    assertEquals('1:10 AM', getStartTimeString());
    assertEquals('3:00 PM', getEndTimeString());
  });

  test('prefStartTime and prefEndTime should have default values', () => {
    // Test that prefStartTime.value and prefEndTime.value are set
    // (crbug.com/1232075).
    slider = document.createElement('settings-scheduler-slider');
    flush();
    const kDefaultStartTimeOffsetMinutes = 18 * 60;
    const kDefaultEndTimeOffsetMinutes = 6 * 60;
    assertNotEquals(undefined, slider.prefStartTime);
    assertNotEquals(undefined, slider.prefEndTime);
    assertEquals(kDefaultStartTimeOffsetMinutes, slider.prefStartTime.value);
    assertEquals(kDefaultEndTimeOffsetMinutes, slider.prefEndTime.value);
  });

  // TODO(crbug.com/1305868): Skip test as it consistently fails whenever
  // daylight savings is active.
  test.skip('pref value update aria label', () => {
    // Test that the aria label is updated after the pref is saved.
    const startKnob = slider.shadowRoot!.querySelector('#startKnob');
    assertTrue(!!startKnob);

    const endKnob = slider.shadowRoot!.querySelector('#endKnob');
    assertTrue(!!endKnob);

    const getStartTimeAriaLabel = () => {
      return startKnob.ariaLabel!.trim();
    };

    const getEndTimeAriaLabel = () => {
      return endKnob.ariaLabel!.trim();
    };

    assertEquals(slider.i18n('startTime', '1:00 AM'), getStartTimeAriaLabel());
    assertEquals(slider.i18n('endTime', '2:00 AM'), getEndTimeAriaLabel());

    slider.prefStartTime.value = 71;
    slider.setPrefValue(
        'ash.night_light.custom_start_time', slider.prefStartTime.value);
    slider.prefEndTime.value = 980;
    slider.setPrefValue(
        'ash.night_light.custom_end_time', slider.prefEndTime.value);
    flush();

    assertEquals(slider.i18n('startTime', '1:11 AM'), getStartTimeAriaLabel());
    assertEquals(slider.i18n('endTime', '4:20 PM'), getEndTimeAriaLabel());
  });
});
