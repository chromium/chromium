// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeInputDeviceSettingsProvider, fakePointingSticks, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsDropdownMenuElement, SettingsPerDevicePointingStickSubsectionElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

const POINTING_STICK_SPEED_SETTING_ID = 435;

suite('<settings-per-device-pointing-stick-subsection>', () => {
  let subsection: SettingsPerDevicePointingStickSubsectionElement;
  let provider: FakeInputDeviceSettingsProvider;

  setup(async () => {
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakePointingSticks(fakePointingSticks);
    setInputDeviceSettingsProviderForTesting(provider);
    subsection =
        document.createElement('settings-per-device-pointing-stick-subsection');
    assert(subsection);
    subsection.set('pointingStick', {...fakePointingSticks[0]});
    document.body.appendChild(subsection);
    await flushTasks();
  });

  teardown(() => {
    subsection.remove();
  });

  /**
   * Test that API are updated when pointing stick settings change.
   */
  test('Update API when pointing stick settings change', async () => {
    const pointingStickSwapButtonDropdown =
        subsection.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#pointingStickSwapButtonDropdown');
    assert(pointingStickSwapButtonDropdown);
    pointingStickSwapButtonDropdown.set('pref.value', true);

    await flushTasks();
    let updatedPointingSticks =
        await provider.getConnectedPointingStickSettings();
    assertEquals(
        updatedPointingSticks[0]!.settings.swapRight,
        pointingStickSwapButtonDropdown.pref!.value);

    const pointingStickAccelerationToggleButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#pointingStickAcceleration');
    pointingStickAccelerationToggleButton!.click();
    await flushTasks();
    updatedPointingSticks = await provider.getConnectedPointingStickSettings();
    assertEquals(
        updatedPointingSticks[0]!.settings.accelerationEnabled,
        pointingStickAccelerationToggleButton!.pref!.value);

    const pointingStickSpeedSlider =
        subsection.shadowRoot!.querySelector<SettingsSliderElement>(
            '#pointingStickSpeedSlider');
    assert(pointingStickSpeedSlider);
    pressAndReleaseKeyOn(
        pointingStickSpeedSlider.shadowRoot!.querySelector('cr-slider')!, 39,
        [], 'ArrowRight');
    await flushTasks();
    updatedPointingSticks = await provider.getConnectedPointingStickSettings();
    assertEquals(
        updatedPointingSticks[0]!.settings.sensitivity,
        pointingStickSpeedSlider.pref!.value);
  });

  /**
   * Test that pointing stick settings data are from the pointing stick
   * provider.
   */
  test('Verify pointing stick settings data', async () => {
    let pointingStickSwapButtonDropdown =
        subsection.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#pointingStickSwapButtonDropdown');
    assertEquals(
        fakePointingSticks[0]!.settings.swapRight,
        pointingStickSwapButtonDropdown!.pref!.value);
    let pointingStickAccelerationToggleButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#pointingStickAcceleration');
    assertEquals(
        fakePointingSticks[0]!.settings.accelerationEnabled,
        pointingStickAccelerationToggleButton!.pref!.value);
    let pointingStickSpeedSlider =
        subsection.shadowRoot!.querySelector<SettingsSliderElement>(
            '#pointingStickSpeedSlider');
    assert(pointingStickSpeedSlider);
    assertEquals(
        fakePointingSticks[0]!.settings.sensitivity,
        pointingStickSpeedSlider.pref.value);

    subsection.set('pointingStick', fakePointingSticks[1]);
    await flushTasks();
    pointingStickSwapButtonDropdown = subsection.shadowRoot!.querySelector(
        '#pointingStickSwapButtonDropdown');
    assertEquals(
        fakePointingSticks[1]!.settings.swapRight,
        pointingStickSwapButtonDropdown!.pref!.value);
    pointingStickAccelerationToggleButton =
        subsection.shadowRoot!.querySelector('#pointingStickAcceleration');
    assertEquals(
        fakePointingSticks[1]!.settings.accelerationEnabled,
        pointingStickAccelerationToggleButton!.pref!.value);
    pointingStickSpeedSlider =
        subsection.shadowRoot!.querySelector('#pointingStickSpeedSlider');
    assert(pointingStickSpeedSlider);
    assertEquals(
        fakePointingSticks[1]!.settings.sensitivity,
        pointingStickSpeedSlider.pref.value);
  });

  /**
   * Verify entering the page with search tags matched will auto focus the
   * searched element.
   */
  test('deep linking mixin focus on the first searched element', async () => {
    const pointingStickSpeedSlider =
        subsection.shadowRoot!.querySelector<SettingsSliderElement>(
            '#pointingStickSpeedSlider');
    subsection.set('pointingStickIndex', 0);
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=trackpoint+speed&settingId=' +
        encodeURIComponent(POINTING_STICK_SPEED_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_POINTING_STICK,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    assert(pointingStickSpeedSlider);
    await waitAfterNextRender(pointingStickSpeedSlider);
    assertEquals(
        pointingStickSpeedSlider, subsection.shadowRoot!.activeElement);
  });

  /**
   * Verify entering the page with search tags matched wll not auto focus the
   * searched element if it's not the first keyboard displayed.
   */
  test('deep linkng mixin does not focus on second element', async () => {
    const pointingStickSpeedSlider =
        subsection.shadowRoot!.querySelector<SettingsSliderElement>(
            '#pointingStickSpeedSlider');
    subsection.set('pointingStickIndex', 1);
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=trackpoint+speed&settingId=' +
        encodeURIComponent(POINTING_STICK_SPEED_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_POINTING_STICK,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    await flushTasks();

    assert(pointingStickSpeedSlider);
    await waitAfterNextRender(pointingStickSpeedSlider);
    assertEquals(null, subsection.shadowRoot!.activeElement);
  });
});
