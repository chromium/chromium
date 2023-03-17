// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {FakeInputDeviceSettingsProvider, fakeTouchpads, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsPerDeviceTouchpadSubsectionElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

const TOUCHPAD_SPEED_SETTING_ID = 405;

suite('PerDeviceTouchpadSubsection', function() {
  /**
   * @type {?SettingsPerDeviceTouchpadSubsectionElement}
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
  function initializePerDeviceTouchpadSubsection() {
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeTouchpads(fakeTouchpads);
    setInputDeviceSettingsProviderForTesting(provider);
    subsection =
        document.createElement('settings-per-device-touchpad-subsection');
    assertTrue(subsection != null);
    subsection.touchpad = {...fakeTouchpads[0]};
    subsection.allowScrollSettings_ = true;
    document.body.appendChild(subsection);
    return flushTasks();
  }

  /**
   * @param {!Boolean} isHaptic
   * @return {!Promise}
   */
  function changeIsHapticState(isHaptic) {
    subsection.touchpad = {...subsection.touchpad, isHaptic: isHaptic};
    return flushTasks();
  }

  /**
   * @param {!Object} touchpad
   * @param {!Boolean} allowScrollSettings
   * @return {!Promise}
   */
  function changeTouchpadSubsectionState(touchpad, allowScrollSettings) {
    subsection.touchpad = touchpad;
    subsection.allowScrollSettings_ = allowScrollSettings;
    return flushTasks();
  }

  async function getConnectedTouchpadSettings() {
    const touchpads = await provider.getConnectedTouchpadSettings();
    return touchpads;
  }

  // Test that API are updated when touchpad settings change.
  test('Update API when touchpad settings change', async () => {
    await initializePerDeviceTouchpadSubsection();
    const enableTapToClickButton =
        subsection.shadowRoot.querySelector('#enableTapToClick');
    enableTapToClickButton.click();
    await flushTasks();
    let updatedTouchpads = await getConnectedTouchpadSettings();
    assertEquals(
        updatedTouchpads[0].settings.tapToClickEnabled,
        enableTapToClickButton.pref.value);

    const enableTapDraggingButton =
        subsection.shadowRoot.querySelector('#enableTapDragging');
    enableTapDraggingButton.click();
    await flushTasks();
    updatedTouchpads = await getConnectedTouchpadSettings();
    assertEquals(
        updatedTouchpads[0].settings.tapDraggingEnabled,
        enableTapDraggingButton.pref.value);

    const touchpadAccelerationButton =
        subsection.shadowRoot.querySelector('#touchpadAcceleration');
    touchpadAccelerationButton.click();
    await flushTasks();
    updatedTouchpads = await getConnectedTouchpadSettings();
    assertEquals(
        updatedTouchpads[0].settings.accelerationEnabled,
        touchpadAccelerationButton.pref.value);

    const touchpadScrollAccelerationButton =
        subsection.shadowRoot.querySelector('#touchpadScrollAcceleration');
    touchpadScrollAccelerationButton.click();
    await flushTasks();
    updatedTouchpads = await getConnectedTouchpadSettings();
    assertEquals(
        updatedTouchpads[0].settings.scrollAcceleration,
        touchpadScrollAccelerationButton.pref.value);

    const touchpadScrollSpeedSlider = assert(
        subsection.shadowRoot.querySelector('#touchpadScrollSpeedSlider'));
    MockInteractions.pressAndReleaseKeyOn(
        touchpadScrollSpeedSlider.shadowRoot.querySelector('cr-slider'),
        39 /* right */, [], 'ArrowRight');
    await flushTasks();
    updatedTouchpads = await getConnectedTouchpadSettings();
    assertEquals(
        updatedTouchpads[0].settings.scrollSensitivity,
        touchpadScrollSpeedSlider.pref.value);

    const touchpadSensitivitySlider =
        assert(subsection.shadowRoot.querySelector('#touchpadSensitivity'));
    MockInteractions.pressAndReleaseKeyOn(
        touchpadSensitivitySlider.shadowRoot.querySelector('cr-slider'),
        39 /* right */, [], 'ArrowRight');
    await flushTasks();
    updatedTouchpads = await getConnectedTouchpadSettings();
    assertEquals(
        updatedTouchpads[0].settings.sensitivity,
        touchpadSensitivitySlider.pref.value);

    const touchpadHapticClickSensitivitySlider = assert(
        subsection.shadowRoot.querySelector('#touchpadHapticClickSensitivity'));
    MockInteractions.pressAndReleaseKeyOn(
        touchpadHapticClickSensitivitySlider.shadowRoot.querySelector(
            'cr-slider'),
        39 /* right */, [], 'ArrowRight');
    await flushTasks();
    updatedTouchpads = await getConnectedTouchpadSettings();
    assertEquals(
        updatedTouchpads[0].settings.hapticSensitivity,
        touchpadHapticClickSensitivitySlider.pref.value);

    const touchpadHapticFeedbackToggleButton =
        subsection.shadowRoot.querySelector('#touchpadHapticFeedbackToggle');
    touchpadHapticFeedbackToggleButton.click();
    await flushTasks();
    updatedTouchpads = await getConnectedTouchpadSettings();
    assertEquals(
        updatedTouchpads[0].settings.hapticEnabled,
        touchpadHapticFeedbackToggleButton.checked);

    const touchpadReverseScrollToggleButton =
        subsection.shadowRoot.querySelector('#enableReverseScrollingToggle');
    touchpadReverseScrollToggleButton.click();
    await flushTasks();
    updatedTouchpads = await getConnectedTouchpadSettings();
    assertEquals(
        updatedTouchpads[0].settings.reverseScrolling,
        touchpadReverseScrollToggleButton.checked);
  });

  // Test that touchpad settings data are from the touchpad provider.
  test('Verify touchpad settings data', async () => {
    await initializePerDeviceTouchpadSubsection();
    let enableTapToClickButton =
        subsection.shadowRoot.querySelector('#enableTapToClick');
    assertEquals(
        fakeTouchpads[0].settings.tapToClickEnabled,
        enableTapToClickButton.pref.value);
    let enableTapDraggingButton =
        subsection.shadowRoot.querySelector('#enableTapDragging');
    assertEquals(
        fakeTouchpads[0].settings.tapDraggingEnabled,
        enableTapDraggingButton.pref.value);
    let touchpadAcceleration =
        subsection.shadowRoot.querySelector('#touchpadAcceleration');
    assertEquals(
        fakeTouchpads[0].settings.accelerationEnabled,
        touchpadAcceleration.pref.value);
    let touchpadScrollAccelerationButton =
        subsection.shadowRoot.querySelector('#touchpadScrollAcceleration');
    assertTrue(isVisible(touchpadScrollAccelerationButton));
    assertEquals(
        fakeTouchpads[0].settings.scrollAcceleration,
        touchpadScrollAccelerationButton.pref.value);
    let touchpadScrollSpeedSlider = assert(
        subsection.shadowRoot.querySelector('#touchpadScrollSpeedSlider'));
    assertTrue(isVisible(touchpadScrollSpeedSlider));
    assertEquals(
        fakeTouchpads[0].settings.scrollSensitivity,
        touchpadScrollSpeedSlider.pref.value);
    let touchpadSensitivitySlider =
        assert(subsection.shadowRoot.querySelector('#touchpadSensitivity'));
    assertEquals(
        fakeTouchpads[0].settings.sensitivity,
        touchpadSensitivitySlider.pref.value);
    let touchpadHapticClickSensitivitySlider = assert(
        subsection.shadowRoot.querySelector('#touchpadHapticClickSensitivity'));
    assertTrue(isVisible(touchpadHapticClickSensitivitySlider));
    assertEquals(
        fakeTouchpads[0].settings.hapticSensitivity,
        touchpadHapticClickSensitivitySlider.pref.value);
    let touchpadHapticFeedbackToggleButton =
        subsection.shadowRoot.querySelector('#touchpadHapticFeedbackToggle');
    assertTrue(isVisible(touchpadHapticFeedbackToggleButton));
    assertEquals(
        fakeTouchpads[0].settings.hapticEnabled,
        touchpadHapticFeedbackToggleButton.checked);
    assertEquals(
        fakeTouchpads[0].settings.reverseScrolling,
        subsection.reverseScrollValue);

    await changeTouchpadSubsectionState(fakeTouchpads[1], false);
    enableTapToClickButton =
        subsection.shadowRoot.querySelector('#enableTapToClick');
    assertEquals(
        fakeTouchpads[1].settings.tapToClickEnabled,
        enableTapToClickButton.pref.value);
    enableTapDraggingButton =
        subsection.shadowRoot.querySelector('#enableTapDragging');
    assertEquals(
        fakeTouchpads[1].settings.tapDraggingEnabled,
        enableTapDraggingButton.pref.value);
    touchpadAcceleration =
        subsection.shadowRoot.querySelector('#touchpadAcceleration');
    assertEquals(
        fakeTouchpads[1].settings.accelerationEnabled,
        touchpadAcceleration.pref.value);
    touchpadScrollAccelerationButton =
        subsection.shadowRoot.querySelector('#touchpadScrollAcceleration');
    assertFalse(isVisible(touchpadScrollAccelerationButton));
    touchpadScrollSpeedSlider =
        subsection.shadowRoot.querySelector('#touchpadScrollSpeedSlider');
    assertFalse(isVisible(touchpadScrollSpeedSlider));
    touchpadSensitivitySlider =
        assert(subsection.shadowRoot.querySelector('#touchpadSensitivity'));
    assertEquals(
        fakeTouchpads[1].settings.sensitivity,
        touchpadSensitivitySlider.pref.value);
    touchpadHapticClickSensitivitySlider =
        subsection.shadowRoot.querySelector('#touchpadHapticClickSensitivity');
    assertFalse(isVisible(touchpadHapticClickSensitivitySlider));
    touchpadHapticFeedbackToggleButton =
        subsection.shadowRoot.querySelector('#touchpadHapticFeedbackToggle');
    assertFalse(isVisible(touchpadHapticFeedbackToggleButton));
    assertEquals(
        fakeTouchpads[1].settings.reverseScrolling,
        subsection.reverseScrollValue);
  });

  /**
   * Test haptic settings are correctly show or hidden based on the touchpad is
   * haptic or not.
   */
  test('Verify haptic settings visbility', async () => {
    await initializePerDeviceTouchpadSubsection();
    // Change the isHaptic state to true.
    await changeIsHapticState(true);
    // Verify haptic click sensitivity slider is visible in the page.
    let hapticClickSensitivitySlider =
        subsection.shadowRoot.querySelector('#touchpadHapticClickSensitivity');
    assertTrue(isVisible(hapticClickSensitivitySlider));

    // Verify haptic feedback toggle button is visible in the page.
    let hapticFeedbackToggleButton =
        subsection.shadowRoot.querySelector('#touchpadHapticFeedbackToggle');
    assertTrue(isVisible(hapticFeedbackToggleButton));

    // Change the isHaptic state to false.
    await changeIsHapticState(false);
    // Verify haptic click sensitivity slider is not visible in the page.
    hapticClickSensitivitySlider =
        subsection.shadowRoot.querySelector('#touchpadHapticClickSensitivity');
    assertFalse(isVisible(hapticClickSensitivitySlider));

    // Verify haptic feedback toggle button is not visible in the page.
    hapticFeedbackToggleButton =
        subsection.shadowRoot.querySelector('#touchpadHapticFeedbackToggle');
    assertFalse(isVisible(hapticFeedbackToggleButton));
  });

  /**
   * Verify entering the page with search tags matched will auto focus the
   * searched element.
   */
  test('deep linking mixin focus on the first searched element', async () => {
    await initializePerDeviceTouchpadSubsection();
    const touchpadSensitivitySlider =
        subsection.shadowRoot.querySelector('#touchpadSensitivity');
    subsection.touchpadIndex = 0;
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=touchpad+speed&settingId=' +
        encodeURIComponent(TOUCHPAD_SPEED_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_TOUCHPAD,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    await waitAfterNextRender(touchpadSensitivitySlider);
    assertTrue(!!touchpadSensitivitySlider);
    assertEquals(
        subsection.shadowRoot.activeElement, touchpadSensitivitySlider);
  });

  /**
   * Verify entering the page with search tags matched wll not auto focus the
   * searched element if it's not the first keyboard displayed.
   */
  test('deep linkng mixin does not focus on second element', async () => {
    await initializePerDeviceTouchpadSubsection();
    const touchpadSensitivitySlider =
        subsection.shadowRoot.querySelector('#touchpadSensitivity');
    subsection.touchpadIndex = 1;
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=touchpad+speed&settingId=' +
        encodeURIComponent(TOUCHPAD_SPEED_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_TOUCHPAD,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    await flushTasks();

    assertTrue(!!touchpadSensitivitySlider);
    assertFalse(!!subsection.shadowRoot.activeElement);
  });
});
