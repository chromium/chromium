// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {FakeInputDeviceSettingsProvider, fakePointingSticks, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsPerDevicePointingStickSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

const POINTING_STICK_SPEED_SETTING_ID = 435;

suite('PerDevicePointingStickSubsection', function() {
  /**
   * @type {?SettingsPerDevicePointingStickSubsectionElement}
   */
  let subsection = null;
  /**
   * @type {?FakeInputDeviceSettingsProvider}
   */
  let provider = null;

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    subsection = null;
    provider = null;
  });

  /**
   * @return {!Promise}
   */
  function initializePerDevicePointingStickSubsection() {
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakePointingSticks(fakePointingSticks);
    setInputDeviceSettingsProviderForTesting(provider);
    subsection =
        document.createElement('settings-per-device-pointing-stick-subsection');
    assertTrue(subsection != null);
    subsection.pointingStick = {...fakePointingSticks[0]};
    document.body.appendChild(subsection);
    return flushTasks();
  }

  /**
   * @param {!Object} pointingStick
   * @return {!Promise}
   */
  function changePointingStickSubsectionState(pointingStick) {
    subsection.pointingStick = pointingStick;
    return flushTasks();
  }

  async function getConnectedPointingStickSettings() {
    const pointingSticks = await provider.getConnectedPointingStickSettings();
    return pointingSticks;
  }

  // Test that API are updated when pointing stick settings change.
  test('Update API when pointing stick settings change', async () => {
    await initializePerDevicePointingStickSubsection();
    const pointingStickSwapButtonDropdown =
        subsection.shadowRoot.querySelector('#pointingStickSwapButtonDropdown');
    pointingStickSwapButtonDropdown.pref = {
      ...pointingStickSwapButtonDropdown.pref,
      value: true,
    };
    await flushTasks();
    let updatedPointingSticks = await getConnectedPointingStickSettings();
    assertEquals(
        updatedPointingSticks[0].settings.swapRight,
        pointingStickSwapButtonDropdown.pref.value);

    const pointingStickAccelerationToggleButton =
        subsection.shadowRoot.querySelector('#pointingStickAcceleration');
    pointingStickAccelerationToggleButton.click();
    await flushTasks();
    updatedPointingSticks = await getConnectedPointingStickSettings();
    assertEquals(
        updatedPointingSticks[0].settings.accelerationEnabled,
        pointingStickAccelerationToggleButton.pref.value);

    const pointingStickSpeedSlider = assert(
        subsection.shadowRoot.querySelector('#pointingStickSpeedSlider'));
    MockInteractions.pressAndReleaseKeyOn(
        pointingStickSpeedSlider.shadowRoot.querySelector('cr-slider'),
        39 /* right */, [], 'ArrowRight');
    await flushTasks();
    updatedPointingSticks = await getConnectedPointingStickSettings();
    assertEquals(
        updatedPointingSticks[0].settings.sensitivity,
        pointingStickSpeedSlider.pref.value);
  });

  // Test that pointing stick settings data are from the pointing stick
  // provider.
  test('Verify pointing stick settings data', async () => {
    await initializePerDevicePointingStickSubsection();
    let pointingStickSwapButtonDropdown =
        subsection.shadowRoot.querySelector('#pointingStickSwapButtonDropdown');
    assertEquals(
        fakePointingSticks[0].settings.swapRight,
        pointingStickSwapButtonDropdown.pref.value);
    let pointingStickAccelerationToggleButton =
        subsection.shadowRoot.querySelector('#pointingStickAcceleration');
    assertEquals(
        fakePointingSticks[0].settings.accelerationEnabled,
        pointingStickAccelerationToggleButton.pref.value);
    let pointingStickSpeedSlider = assert(
        subsection.shadowRoot.querySelector('#pointingStickSpeedSlider'));
    assertEquals(
        fakePointingSticks[0].settings.sensitivity,
        pointingStickSpeedSlider.pref.value);

    await changePointingStickSubsectionState(fakePointingSticks[1]);
    pointingStickSwapButtonDropdown =
        subsection.shadowRoot.querySelector('#pointingStickSwapButtonDropdown');
    assertEquals(
        fakePointingSticks[1].settings.swapRight,
        pointingStickSwapButtonDropdown.pref.value);
    pointingStickAccelerationToggleButton =
        subsection.shadowRoot.querySelector('#pointingStickAcceleration');
    assertEquals(
        fakePointingSticks[1].settings.accelerationEnabled,
        pointingStickAccelerationToggleButton.pref.value);
    pointingStickSpeedSlider = assert(
        subsection.shadowRoot.querySelector('#pointingStickSpeedSlider'));
    assertEquals(
        fakePointingSticks[1].settings.sensitivity,
        pointingStickSpeedSlider.pref.value);
  });

  /**
   * Verify entering the page with search tags matched will auto focus the
   * searched element.
   */
  test('deep linking mixin focus on the first searched element', async () => {
    await initializePerDevicePointingStickSubsection();
    const pointingStickSpeedSlider =
        subsection.shadowRoot.querySelector('#pointingStickSpeedSlider');
    subsection.pointingStickIndex = 0;
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=trackpoint+speed&settingId=' +
        encodeURIComponent(POINTING_STICK_SPEED_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_POINTING_STICK,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    await waitAfterNextRender(pointingStickSpeedSlider);
    assertTrue(!!pointingStickSpeedSlider);
    assertEquals(subsection.shadowRoot.activeElement, pointingStickSpeedSlider);
  });

  /**
   * Verify entering the page with search tags matched wll not auto focus the
   * searched element if it's not the first keyboard displayed.
   */
  test('deep linkng mixin does not focus on second element', async () => {
    await initializePerDevicePointingStickSubsection();
    const pointingStickSpeedSlider =
        subsection.shadowRoot.querySelector('#pointingStickSpeedSlider');
    subsection.pointingStickIndex = 1;
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=trackpoint+speed&settingId=' +
        encodeURIComponent(POINTING_STICK_SPEED_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_POINTING_STICK,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    await flushTasks();

    assertTrue(!!pointingStickSpeedSlider);
    assertFalse(!!subsection.shadowRoot.activeElement);
  });
});
