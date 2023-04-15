// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CrToggleElement, FakeInputDeviceSettingsProvider, fakeMice, Mouse, PolicyStatus, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsDropdownMenuElement, SettingsPerDeviceMouseSubsectionElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

const MOUSE_ACCELERATION_SETTING_ID = 408;

suite('<settings-per-device-mouse-subsection>', function() {
  let subsection: SettingsPerDeviceMouseSubsectionElement;
  let provider: FakeInputDeviceSettingsProvider;

  teardown(() => {
    subsection.remove();
  });

  function initializePerDeviceMouseSubsection(): Promise<void> {
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeMice(fakeMice);
    setInputDeviceSettingsProviderForTesting(provider);
    subsection = document.createElement('settings-per-device-mouse-subsection');
    assert(subsection);
    subsection.set('mouse', {...fakeMice[0]});
    subsection.set('allowScrollSettings_', true);
    document.body.appendChild(subsection);
    return flushTasks();
  }

  function changeMouseSubsectionState(
      mouse: Mouse, allowScrollSettings: boolean): Promise<void> {
    subsection.set('mouse', mouse);
    subsection.set('allowScrollSettings_', allowScrollSettings);
    return flushTasks();
  }

  /**
   * Test that API are updated when mouse settings change.
   */
  test('Update API when mouse settings change', async () => {
    await initializePerDeviceMouseSubsection();
    const mouseSwapButtonDropdown =
        subsection.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#mouseSwapButtonDropdown');
    assert(mouseSwapButtonDropdown);
    await flushTasks();
    let updatedMice = await provider.getConnectedMouseSettings();
    assertEquals(
        updatedMice[0]!.settings.swapRight,
        mouseSwapButtonDropdown.pref!.value);

    const mouseAccelerationToggleButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseAcceleration');
    assert(mouseAccelerationToggleButton);
    mouseAccelerationToggleButton.click();
    await flushTasks();
    updatedMice = await provider.getConnectedMouseSettings();
    assertEquals(
        updatedMice[0]!.settings.accelerationEnabled,
        mouseAccelerationToggleButton.pref!.value);

    const mouseSpeedSlider =
        subsection.shadowRoot!.querySelector<SettingsSliderElement>(
            '#mouseSpeedSlider');
    assert(mouseSpeedSlider);
    const arrowRightEvent =
        new KeyboardEvent('keypress', {'key': 'ArrowRight'});
    mouseSpeedSlider.shadowRoot!.querySelector('cr-slider')!.dispatchEvent(
        arrowRightEvent);
    await flushTasks();
    updatedMice = await provider.getConnectedMouseSettings();
    assertEquals(
        updatedMice[0]!.settings.sensitivity, mouseSpeedSlider.pref!.value);

    const mouseReverseScrollToggleButton =
        subsection.shadowRoot!.querySelector<CrToggleElement>(
            '#mouseReverseScroll');
    assert(mouseReverseScrollToggleButton);
    mouseReverseScrollToggleButton.click();
    await flushTasks();
    updatedMice = await provider.getConnectedMouseSettings();
    assertEquals(
        updatedMice[0]!.settings.reverseScrolling,
        mouseReverseScrollToggleButton.checked);

    const mouseScrollAccelerationToggleButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseScrollAcceleration');
    assert(mouseScrollAccelerationToggleButton);
    mouseScrollAccelerationToggleButton.click();
    await flushTasks();
    updatedMice = await provider.getConnectedMouseSettings();
    assertEquals(
        updatedMice[0]!.settings.scrollAcceleration,
        mouseScrollAccelerationToggleButton.pref!.value);

    const mouseScrollSpeedSlider =
        subsection.shadowRoot!.querySelector<SettingsSliderElement>(
            '#mouseScrollSpeedSlider');
    assert(mouseScrollSpeedSlider);
    mouseScrollSpeedSlider.shadowRoot!.querySelector('cr-slider')!
        .dispatchEvent(arrowRightEvent);
    await flushTasks();
    updatedMice = await provider.getConnectedMouseSettings();
    assertEquals(
        updatedMice[0]!.settings.scrollSensitivity,
        mouseScrollSpeedSlider.pref!.value);
  });

  /**
   * Test that mouse settings data are from the mouse provider.
   */
  test('Verify mouse settings data', async () => {
    await initializePerDeviceMouseSubsection();
    let mouseSwapButtonDropdown =
        subsection.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#mouseSwapButtonDropdown');
    assertEquals(
        fakeMice[0]!.settings.swapRight, mouseSwapButtonDropdown!.pref!.value);
    let mouseAccelerationToggleButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseAcceleration');
    assertEquals(
        fakeMice[0]!.settings.accelerationEnabled,
        mouseAccelerationToggleButton!.pref!.value);
    let mouseSpeedSlider =
        subsection.shadowRoot!.querySelector<SettingsSliderElement>(
            '#mouseSpeedSlider');
    assertEquals(
        fakeMice[0]!.settings.sensitivity, mouseSpeedSlider!.pref!.value);
    assertEquals(
        fakeMice[0]!.settings.reverseScrolling,
        subsection.get('reverseScrollValue'));
    let mouseScrollAccelerationToggleButton =
        subsection.shadowRoot!.querySelector<SettingsSliderElement>(
            '#mouseScrollAcceleration');
    assertTrue(isVisible(mouseScrollAccelerationToggleButton));
    assertEquals(
        fakeMice[0]!.settings.scrollAcceleration,
        mouseScrollAccelerationToggleButton!.pref!.value);
    let mouseScrollSpeedSlider =
        subsection.shadowRoot!.querySelector<SettingsSliderElement>(
            '#mouseScrollSpeedSlider');
    assert(mouseScrollSpeedSlider);
    assertTrue(isVisible(mouseScrollSpeedSlider));
    assertEquals(
        fakeMice[0]!.settings.scrollSensitivity,
        mouseScrollSpeedSlider.pref!.value);

    assert(fakeMice[1]);
    await changeMouseSubsectionState(fakeMice[1], false);
    mouseSwapButtonDropdown =
        subsection.shadowRoot!.querySelector('#mouseSwapButtonDropdown');
    assertEquals(
        fakeMice[1]!.settings.swapRight, mouseSwapButtonDropdown!.pref!.value);
    mouseAccelerationToggleButton =
        subsection.shadowRoot!.querySelector('#mouseAcceleration');
    assertEquals(
        fakeMice[1]!.settings.accelerationEnabled,
        mouseAccelerationToggleButton!.pref!.value);
    mouseSpeedSlider =
        subsection.shadowRoot!.querySelector('#mouseSpeedSlider');
    assertEquals(
        fakeMice[1]!.settings.sensitivity, mouseSpeedSlider!.pref!.value);
    assertEquals(
        fakeMice[1]!.settings.reverseScrolling,
        subsection.get('reverseScrollValue'));
    mouseScrollAccelerationToggleButton =
        subsection.shadowRoot!.querySelector('#mouseScrollAcceleration');
    assertFalse(isVisible(mouseScrollAccelerationToggleButton));
    mouseScrollSpeedSlider =
        subsection.shadowRoot!.querySelector('#mouseScrollSpeedSlider');
    assertFalse(isVisible(mouseScrollSpeedSlider));
  });

  /**
   * Verify entering the page with search tags matched will auto focus the
   * searched element.
   */
  test('deep linking mixin focus on the first searched element', async () => {
    await initializePerDeviceMouseSubsection();
    const mouseAccelerationToggle =
        subsection.shadowRoot!.querySelector<HTMLElement>('#mouseAcceleration');
    subsection.set('mouseIndex', 0);
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=mouse+accel&settingId=' +
        encodeURIComponent(MOUSE_ACCELERATION_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_MOUSE,
        /* dynamicParams= */ url, /* removeSearch= */ true);

    assert(mouseAccelerationToggle);
    await waitAfterNextRender(mouseAccelerationToggle);
    assertEquals(subsection.shadowRoot!.activeElement, mouseAccelerationToggle);
  });

  /**
   * Verify entering the page with search tags matched wll not auto focus the
   * searched element if it's not the first keyboard displayed.
   */
  test('deep linking mixin does not focus on second element', async () => {
    await initializePerDeviceMouseSubsection();
    const mouseAccelerationToggle =
        subsection.shadowRoot!.querySelector('#mouseAcceleration');
    subsection.set('mouseIndex', 1);
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=mouse+accel&settingId=' +
        encodeURIComponent(MOUSE_ACCELERATION_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_MOUSE,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    await flushTasks();

    assert(mouseAccelerationToggle);
    assertEquals(null, subsection.shadowRoot!.activeElement);
  });

  /**
   * Verifies that the policy indicator is properly reflected in the UI.
   */
  test('swap right policy reflected in UI', async () => {
    await initializePerDeviceMouseSubsection();
    subsection.set('mousePolicies', {
      swapRightPolicy: {policy_status: PolicyStatus.kManaged, value: false},
    });
    await flushTasks();
    const swapRightDropdown =
        subsection.shadowRoot!.querySelector('#mouseSwapButtonDropdown');
    assert(swapRightDropdown);
    let policyIndicator =
        swapRightDropdown.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertTrue(isVisible(policyIndicator));

    subsection.set('mousePolicies', {swapRightPolicy: undefined});
    await flushTasks();
    policyIndicator =
        swapRightDropdown.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertFalse(isVisible(policyIndicator));
  });
});
