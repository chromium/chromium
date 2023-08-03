// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CrToggleElement, FakeInputDeviceSettingsProvider, fakeMice, Mouse, PolicyStatus, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsDropdownMenuElement, SettingsPerDeviceMouseSubsectionElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
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
        subsection.shadowRoot!.querySelector<CrToggleElement>(
            '#mouseScrollAcceleration');
    assert(mouseScrollAccelerationToggleButton);
    mouseScrollAccelerationToggleButton.click();
    await flushTasks();
    updatedMice = await provider.getConnectedMouseSettings();
    assertEquals(
        updatedMice[0]!.settings.scrollAcceleration,
        mouseScrollAccelerationToggleButton.checked);

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
        subsection.shadowRoot!.querySelector<CrToggleElement>(
            '#mouseScrollAcceleration');
    assertTrue(isVisible(mouseScrollAccelerationToggleButton));
    assertEquals(
        fakeMice[0]!.settings.scrollAcceleration,
        mouseScrollAccelerationToggleButton!.checked);
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

  /**
   * Verify clicking the customize mouse buttons row will be redirecting to the
   * customize mouse buttons subpage.
   */
  test('click customize mouse buttons redirect to new subpage', async () => {
    await initializePerDeviceMouseSubsection();
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
   * Test that turn on scroll acceleration will disable scrolling speed slider.
   */
  test(
      'turn on scroll acceleration will disable scrolling speed slider',
      async () => {
        await initializePerDeviceMouseSubsection();
        const mouseScrollAccelerationToggleButton =
            subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#mouseScrollAcceleration');
        assert(mouseScrollAccelerationToggleButton);
        const mouseScrollSpeedSlider =
            subsection.shadowRoot!.querySelector<SettingsSliderElement>(
                '#mouseScrollSpeedSlider');
        assert(mouseScrollSpeedSlider);

        // When scroll acceleration is off, scroll speed slider is enabled.
        assertFalse(fakeMice[0]!.settings.scrollAcceleration);
        assertFalse(mouseScrollSpeedSlider.disabled);

        mouseScrollAccelerationToggleButton.click();
        // Refresh the whole subsection page is necessary since the slider
        // element has some issue getting updated.
        await initializePerDeviceMouseSubsection();

        // When scroll acceleration is on, scroll speed slider is disaabled.
        assertTrue(fakeMice[0]!.settings.scrollAcceleration);
        const updatedMouseScrollSpeedSlider =
            subsection.shadowRoot!.querySelector<SettingsSliderElement>(
                '#mouseScrollSpeedSlider');
        assert(updatedMouseScrollSpeedSlider);
        assertTrue(updatedMouseScrollSpeedSlider.disabled);
      });
});
