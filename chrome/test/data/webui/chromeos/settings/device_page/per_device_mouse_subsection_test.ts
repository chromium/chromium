// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CrLinkRowElement, CrToggleElement, FakeInputDeviceSettingsProvider, fakeMice, fakeMice2, Mouse, PerDeviceSubsectionHeaderElement, PolicyStatus, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsDropdownMenuElement, SettingsPerDeviceMouseSubsectionElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import type {BluetoothBatteryIconPercentageElement} from 'chrome://resources/ash/common/bluetooth/bluetooth_battery_icon_percentage.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

const MOUSE_ACCELERATION_SETTING_ID = 408;

suite('<settings-per-device-mouse-subsection>', function() {
  let subsection: SettingsPerDeviceMouseSubsectionElement;
  let provider: FakeInputDeviceSettingsProvider;

  setup(() => {
    setPeripheralCustomizationEnabled(true);
    setWelcomeExperienceEnabled(true);
  });

  teardown(() => {
    subsection.remove();
  });

  function initializePerDeviceMouseSubsection(fakeMice: Mouse[]):
      Promise<void> {
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
   * Override enablePeripheralCustomization feature flag.
   * @param {!boolean} isEnabled
   */
  function setPeripheralCustomizationEnabled(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enablePeripheralCustomization: isEnabled,
    });
  }

  function setWelcomeExperienceEnabled(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enableWelcomeExperience: isEnabled,
    });
  }

  /**
   * Test that API are updated when mouse settings change.
   */
  test('Update API when mouse settings change', async () => {
    setPeripheralCustomizationEnabled(false);
    await initializePerDeviceMouseSubsection(fakeMice);
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

    const mouseControlledScrollingToggleButton =
        subsection.shadowRoot!.querySelector<CrToggleElement>(
            '#mouseControlledScrolling');
    assert(mouseControlledScrollingToggleButton);
    mouseControlledScrollingToggleButton.click();
    await flushTasks();
    updatedMice = await provider.getConnectedMouseSettings();
    assertEquals(
        updatedMice[0]!.settings.scrollAcceleration,
        !mouseControlledScrollingToggleButton.checked);

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
   * Test that there is no customizeButtonsRow if the mouse
   * is uncustomizable.
   */
  test('Check if show customize button row', async () => {
    await initializePerDeviceMouseSubsection(fakeMice2);
    const customizeButtonsRow =
        subsection.shadowRoot!.querySelector<CrLinkRowElement>(
            '#customizeMouseButtons');
    assertFalse(!!customizeButtonsRow);
  });

  /**
   * Test that there is mouse swap toggle button if the mouse
   * has kDisallowCustomizations restriction and PeripheralCustomization
   * is enabled.
   */
  test('Check if show mouse swap toggle button', async () => {
    await initializePerDeviceMouseSubsection(fakeMice2);
    let mouseSwapToggleButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseSwapToggleButton');
    let customizeButtonsRow =
        subsection.shadowRoot!.querySelector<CrLinkRowElement>(
            '#customizeMouseButtons');
    assertTrue(!!mouseSwapToggleButton);
    assertTrue(mouseSwapToggleButton!.pref!.value);
    assertEquals(
        fakeMice2[0]!.settings.swapRight, mouseSwapToggleButton!.pref!.value);
    assertFalse(!!customizeButtonsRow);

    // Click mouse swap toggle button will update the pref value.
    mouseSwapToggleButton.click();
    await flushTasks();
    assertFalse(mouseSwapToggleButton!.pref!.value);
    assertEquals(
        fakeMice2[0]!.settings.swapRight, mouseSwapToggleButton!.pref!.value);

    // Turn off the feature flag, the mouse swap toggle button disappear.
    setPeripheralCustomizationEnabled(false);
    await initializePerDeviceMouseSubsection(fakeMice2);
    mouseSwapToggleButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseSwapToggleButton');
    assertFalse(!!mouseSwapToggleButton);
    customizeButtonsRow =
        subsection.shadowRoot!.querySelector<CrLinkRowElement>(
            '#customizeMouseButtons');
    assertFalse(!!customizeButtonsRow);

    // If the customization restriction is not kDisallowCustomizations,
    // the mouse swap toggle button disappear.
    setPeripheralCustomizationEnabled(true);
    await initializePerDeviceMouseSubsection(fakeMice);
    mouseSwapToggleButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseSwapToggleButton');
    assertFalse(!!mouseSwapToggleButton);
    customizeButtonsRow =
        subsection.shadowRoot!.querySelector<CrLinkRowElement>(
            '#customizeMouseButtons');
    assertTrue(!!customizeButtonsRow);
  });

  /**
   * Test that mouse settings data are from the mouse provider.
   */
  test('Verify mouse settings data', async () => {
    await initializePerDeviceMouseSubsection(fakeMice);
    // Verify that swapright setting will not be visible when
    // peripheralCustomization flag is enabled.
    let mouseSwapButtonDropdown =
        subsection.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#mouseSwapButtonDropdown');
    assert(!mouseSwapButtonDropdown);

    setPeripheralCustomizationEnabled(false);
    await initializePerDeviceMouseSubsection(fakeMice);
    mouseSwapButtonDropdown =
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
    let mouseControlledScrollingToggleButton =
        subsection.shadowRoot!.querySelector<CrToggleElement>(
            '#mouseControlledScrolling');
    assertTrue(isVisible(mouseControlledScrollingToggleButton));
    assertEquals(
        fakeMice[0]!.settings.scrollAcceleration,
        !mouseControlledScrollingToggleButton!.checked);
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
    mouseControlledScrollingToggleButton =
        subsection.shadowRoot!.querySelector('#mouseControlledScrolling');
    assertFalse(isVisible(mouseControlledScrollingToggleButton));
    mouseScrollSpeedSlider =
        subsection.shadowRoot!.querySelector('#mouseScrollSpeedSlider');
    assertFalse(isVisible(mouseScrollSpeedSlider));
  });

  /**
   * Verify entering the page with search tags matched will auto focus the
   * searched element.
   */
  test('deep linking mixin focus on the first searched element', async () => {
    await initializePerDeviceMouseSubsection(fakeMice);
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
    await initializePerDeviceMouseSubsection(fakeMice);
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
    setPeripheralCustomizationEnabled(false);
    await initializePerDeviceMouseSubsection(fakeMice);
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

  /**
   * Verify clicking the customize mouse buttons row will be redirecting to the
   * customize mouse buttons subpage.
   */
  test('click customize mouse buttons redirect to new subpage', async () => {
    await initializePerDeviceMouseSubsection(fakeMice);
    const customizeButtonsRow =
        subsection.shadowRoot!.querySelector<CrLinkRowElement>(
            '#customizeMouseButtons');
    assertTrue(!!customizeButtonsRow);
    customizeButtonsRow.click();

    await flushTasks();
    assertEquals(
        routes.CUSTOMIZE_MOUSE_BUTTONS, Router.getInstance().currentRoute);

    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('mouseId');
    assertTrue(!!urlSearchQuery);
    const mouseId = Number(urlSearchQuery);
    assertFalse(isNaN(mouseId));
    const expectedMouseId = subsection.get('mouse.id');
    assertEquals(expectedMouseId, mouseId);
  });

  /**
   * Test that turn on controlled scrolling will enable scrolling speed slider.
   */
  test(
      'turn on controlled scrolling will enable scrolling speed slider',
      async () => {
        await initializePerDeviceMouseSubsection(fakeMice);
        const mouseControlledScrollingToggleButton =
            subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mouseControlledScrolling');
        assert(mouseControlledScrollingToggleButton);
        const mouseScrollSpeedSlider =
            subsection.shadowRoot!.querySelector<SettingsSliderElement>(
                '#mouseScrollSpeedSlider');
        assert(mouseScrollSpeedSlider);

        // When controlled scrolling is on, scroll speed slider is enabled.
        assertFalse(fakeMice[0]!.settings.scrollAcceleration);
        assertFalse(mouseScrollSpeedSlider.disabled);

        mouseControlledScrollingToggleButton.click();
        // Refresh the whole subsection page is necessary since the slider
        // element has some issue getting updated.
        await initializePerDeviceMouseSubsection(fakeMice);

        // When controlled scrolling is off, scroll speed slider is disabled.
        assertTrue(fakeMice[0]!.settings.scrollAcceleration);
        const updatedMouseScrollSpeedSlider =
            subsection.shadowRoot!.querySelector<SettingsSliderElement>(
                '#mouseScrollSpeedSlider');
        assert(updatedMouseScrollSpeedSlider);
        assertTrue(updatedMouseScrollSpeedSlider.disabled);
      });

  test(
      'battery percentage displayed for connected bluetooth devices',
      async () => {
        await initializePerDeviceMouseSubsection(fakeMice);
        const subsectionHeader = strictQuery(
            '#subsectionHeader', subsection.shadowRoot,
            PerDeviceSubsectionHeaderElement);
        const batteryIcon =
            subsectionHeader.shadowRoot!
                .querySelector<BluetoothBatteryIconPercentageElement>(
                    '#batteryIcon');
        assertTrue(isVisible(batteryIcon));
      });

  /**
   * Test that the row to open a companion app is displayed when an app is
   * installed.
   */
  test('Open app row displayed when app is installed', async () => {
    await initializePerDeviceMouseSubsection(fakeMice);
    let appRow = subsection.shadowRoot!.querySelector('#openApp');
    assertFalse(isVisible(appRow));
    subsection.set('mouse', {...fakeMice[1]});
    await flushTasks();
    appRow = subsection.shadowRoot!.querySelector('#AppInstalledRow');
    assertTrue(isVisible(appRow));
  });
});
