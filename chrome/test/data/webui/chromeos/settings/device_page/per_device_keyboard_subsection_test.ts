// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CrLinkRowElement, FakeInputDeviceSettingsProvider, fakeKeyboards, Keyboard, MetaKey, PolicyStatus, Router, routes, setInputDeviceSettingsProviderForTesting, SettingsPerDeviceKeyboardSubsectionElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

const KEYBOARD_FUNCTION_KEYS_SETTING_ID = 411;
const KEYBOARD_SWITCH_TOP_ROW_KEYS_SETTING_ID = 441;

suite('<settings-per-device-keyboard-subsection>', () => {
  let subsection: SettingsPerDeviceKeyboardSubsectionElement;
  let provider: FakeInputDeviceSettingsProvider;

  setup(async () => {
    await initializePerDeviceKeyboardSubsection(
        fakeKeyboards, /*rgbKeyboardSupported=*/ true,
        /*hasKeyboardBacklight=*/ true,
        /*hasAmbientLightSensor=*/ true);
  });

  teardown(() => {
    subsection.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function initializePerDeviceKeyboardSubsection(
      fakeKeyboards: Keyboard[], rgbKeyboardSupported: boolean,
      hasKeyboardBacklight: boolean,
      hasAmbientLightSensor: boolean): Promise<void> {
    provider = new FakeInputDeviceSettingsProvider();
    provider.setFakeKeyboards(fakeKeyboards);
    provider.setFakeIsRgbKeyboardSupported(rgbKeyboardSupported);
    provider.setFakeHasKeyboardBacklight(hasKeyboardBacklight);
    provider.setFakeHasAmbientLightSensor(hasAmbientLightSensor);
    setInputDeviceSettingsProviderForTesting(provider);

    subsection =
        document.createElement('settings-per-device-keyboard-subsection');
    subsection.set('keyboard', {...fakeKeyboards[0]});
    document.body.appendChild(subsection);
    return flushTasks();
  }

  function getElement(selector: string): Element|null {
    return subsection.shadowRoot!.querySelector(selector);
  }

  /**
   * Override enableKeyboardBacklightControlInSettings feature flag.
   * @param {!boolean} isEnabled
   */
  function setKeyboardBacklightControlEnabled(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enableKeyboardBacklightControlInSettings: isEnabled,
    });
  }

  /**
   * Changes the external state of the keyboard.
   */
  function changeIsExternalState(isExternal: boolean): Promise<void> {
    const keyboard = {
      ...subsection.get('keyboard'),
      isExternal: isExternal,
      metaKey: isExternal ? MetaKey.kExternalMeta : MetaKey.kSearch,
    };
    subsection.set('keyboard', keyboard);
    return flushTasks();
  }

  /**
   * Test that API are updated when keyboard settings change.
   */
  test('Update API when keyboard settings change', async () => {
    const externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#externalTopRowAreFunctionKeysButton');
    assert(externalTopRowAreFunctionKeysButton);
    externalTopRowAreFunctionKeysButton!.click();
    await flushTasks();
    let updatedKeyboards = await provider.getConnectedKeyboardSettings();
    assertEquals(
        updatedKeyboards[0]!.settings.topRowAreFkeys,
        externalTopRowAreFunctionKeysButton.pref!.value);

    const blockMetaFunctionKeyRewritesButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#blockMetaFunctionKeyRewritesButton');
    assert(blockMetaFunctionKeyRewritesButton);
    blockMetaFunctionKeyRewritesButton.click();
    await flushTasks();

    updatedKeyboards = await provider.getConnectedKeyboardSettings();

    assertEquals(
        updatedKeyboards[0]!.settings.suppressMetaFkeyRewrites,
        blockMetaFunctionKeyRewritesButton.pref!.value);
  });

  /**Test that keyboard settings data are from the keyboard provider.*/
  test('Verify keyboard settings data', async () => {
    let externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#externalTopRowAreFunctionKeysButton');
    assert(externalTopRowAreFunctionKeysButton);
    assertTrue(isVisible(externalTopRowAreFunctionKeysButton));
    assertEquals(
        fakeKeyboards[0]!.settings.topRowAreFkeys,
        externalTopRowAreFunctionKeysButton.pref!.value);
    let blockMetaFunctionKeyRewritesButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#blockMetaFunctionKeyRewritesButton');
    assert(blockMetaFunctionKeyRewritesButton);
    assertTrue(isVisible(blockMetaFunctionKeyRewritesButton));
    assertEquals(
        fakeKeyboards[0]!.settings.suppressMetaFkeyRewrites,
        blockMetaFunctionKeyRewritesButton.pref!.value);
    let internalTopRowAreFunctionKeysButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#internalTopRowAreFunctionKeysButton');
    assertFalse(isVisible(internalTopRowAreFunctionKeysButton));

    subsection.set('keyboard', fakeKeyboards[1]);
    await flushTasks();

    externalTopRowAreFunctionKeysButton = subsection.shadowRoot!.querySelector(
        '#externalTopRowAreFunctionKeysButton');
    assertFalse(isVisible(externalTopRowAreFunctionKeysButton));
    blockMetaFunctionKeyRewritesButton = subsection.shadowRoot!.querySelector(
        '#blockMetaFunctionKeyRewritesButton');
    assertFalse(isVisible(blockMetaFunctionKeyRewritesButton));
    internalTopRowAreFunctionKeysButton = subsection.shadowRoot!.querySelector(
        '#internalTopRowAreFunctionKeysButton');
    assertTrue(isVisible(internalTopRowAreFunctionKeysButton));
    assertEquals(
        fakeKeyboards[1]!.settings.topRowAreFkeys,
        internalTopRowAreFunctionKeysButton!.pref!.value);
  });

  /**
   * Test that keyboard settings are correctly show or hidden based on internal
   * vs external.
   */
  test('Verify keyboard settings visibility', async () => {
    // Change the isExternal state to true.
    await changeIsExternalState(true);
    // Verify external top-row are function keys toggle button is visible in the
    // page.
    let externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot!.querySelector(
            '#externalTopRowAreFunctionKeysButton');
    assertTrue(isVisible(externalTopRowAreFunctionKeysButton));

    // Verify block meta function key rewrites toggle button is visible in the
    // page.
    let blockMetaFunctionKeyRewritesButton =
        subsection.shadowRoot!.querySelector(
            '#blockMetaFunctionKeyRewritesButton');
    assertTrue(isVisible(blockMetaFunctionKeyRewritesButton));

    // Verify internal top-row are function keys toggle button is not visible in
    // the page.
    let internalTopRowAreFunctionKeysButton =
        subsection.shadowRoot!.querySelector(
            '#internalTopRowAreFunctionKeysButton');
    assertFalse(isVisible(internalTopRowAreFunctionKeysButton));

    // Change the isExternal state to false.
    await changeIsExternalState(false);
    // Verify external top-row are function keys toggle button is not visible in
    // the page.
    externalTopRowAreFunctionKeysButton = subsection.shadowRoot!.querySelector(
        '#externalTopRowAreFunctionKeysButton');
    assertFalse(isVisible(externalTopRowAreFunctionKeysButton));

    // Verify block meta function key rewrites toggle button is not visible in
    // the page.
    blockMetaFunctionKeyRewritesButton = subsection.shadowRoot!.querySelector(
        '#blockMetaFunctionKeyRewritesButton');
    assertFalse(isVisible(blockMetaFunctionKeyRewritesButton));

    // Verify internal top-row are function keys toggle button is visible in the
    // page.
    internalTopRowAreFunctionKeysButton = subsection.shadowRoot!.querySelector(
        '#internalTopRowAreFunctionKeysButton');
    assertTrue(isVisible(internalTopRowAreFunctionKeysButton));
  });

  /**
   * Test that expected html elements are in the page after loaded.
   */
  test('per-device keyboard subsection loaded', () => {
    // Verify the external top-row are function keys toggle button is in the
    // page.
    const externalTopRowAreFunctionKeysButton =
        subsection.shadowRoot!.querySelector(
            '#externalTopRowAreFunctionKeysButton');
    assert(externalTopRowAreFunctionKeysButton);
  });

  /**
   * Verify the Keyboard remap keys row label is loaded, and sub-label is
   * correctly displayed when the keyboard has 2, 1 or 0 remapped keys.
   */
  test('remap keys sub-label displayed correctly', async () => {
    const remapKeysRow =
        subsection.shadowRoot!.querySelector('#remapKeyboardKeys');
    assert(remapKeysRow);
    assertEquals(
        'Customize keyboard keys',
        remapKeysRow.shadowRoot!.querySelector('#label')!.textContent!.trim());

    const remapKeysSubLabel =
        remapKeysRow.shadowRoot!.querySelector('#subLabel');
    assert(remapKeysSubLabel);
    assertEquals(
        2,
        Object.keys(subsection.get('keyboard.settings.modifierRemappings'))
            .length);
    assertEquals('2 customized keys', remapKeysSubLabel.textContent!.trim());

    subsection.set('keyboard', fakeKeyboards[2]);
    await flushTasks();
    assertEquals(
        1,
        Object.keys(subsection.get('keyboard.settings.modifierRemappings'))
            .length);
    assertEquals('1 customized key', remapKeysSubLabel.textContent!.trim());

    subsection.set('keyboard', fakeKeyboards[1]);
    await flushTasks();
    assertEquals(
        0,
        Object.keys(subsection.get('keyboard.settings.modifierRemappings'))
            .length);
    assertEquals('No keys customized', remapKeysSubLabel.textContent!.trim());
    loadTimeData.overrideValues({
      enableAltClickAndSixPackCustomization: true,
    });
    subsection.set('keyboard', fakeKeyboards[3]);
    await flushTasks();
    // Expect 3 remapped six pack key shortcuts and 2 remapped modifier keys.
    assertEquals('5 customized keys', remapKeysSubLabel.textContent!.trim());
  });

  /**
   * Verify clicking the Keyboard remap keys button will be redirecting to the
   * remapped keys subpage.
   */
  test('click remap keys button redirect to new subpage', async () => {
    const remapKeysRow = subsection.shadowRoot!.querySelector<CrLinkRowElement>(
        '#remapKeyboardKeys');
    assert(remapKeysRow);
    remapKeysRow.click();

    await flushTasks();
    assertEquals(
        routes.PER_DEVICE_KEYBOARD_REMAP_KEYS,
        Router.getInstance().currentRoute);

    const urlSearchQuery =
        Router.getInstance().getQueryParameters().get('keyboardId');
    assert(urlSearchQuery);
    const keyboardId = Number(urlSearchQuery);
    assertFalse(isNaN(keyboardId));
    const expectedKeyboardId = subsection.get('keyboard.id');
    assertEquals(expectedKeyboardId, keyboardId);
  });

  /**
   * Verify entering the page with search tags matched will auto focus the
   * searched element.
   */
  test('deep linking mixin focus on the first searched element', async () => {
    const topRowAreFunctionKeysToggle =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#externalTopRowAreFunctionKeysButton');
    assert(topRowAreFunctionKeysToggle);
    subsection.set('keyboardIndex', 0);
    // Enter the page from auto repeat search tag.
    const searchAutoRepeatUrl = new URLSearchParams(
        'search=keyboard&settingId=' +
        encodeURIComponent(KEYBOARD_FUNCTION_KEYS_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_KEYBOARD,
        /* dynamicParams= */ searchAutoRepeatUrl, /* removeSearch= */ true);

    await waitAfterNextRender(topRowAreFunctionKeysToggle);
    assertEquals(
        subsection.shadowRoot!.activeElement, topRowAreFunctionKeysToggle);

    const switchTopRowKeysButton =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#blockMetaFunctionKeyRewritesButton');
    assert(switchTopRowKeysButton);
    const searchSwitchTopRowKeysUrl = new URLSearchParams(
        'search=switch+top&settingId=' +
        encodeURIComponent(KEYBOARD_SWITCH_TOP_ROW_KEYS_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_KEYBOARD,
        /* dynamicParams= */ searchSwitchTopRowKeysUrl,
        /* removeSearch= */ true);

    await waitAfterNextRender(switchTopRowKeysButton);
    assert(switchTopRowKeysButton);
    assertEquals(subsection.shadowRoot!.activeElement, switchTopRowKeysButton);
  });

  /**
   * Verify entering the page with search tags matched wll not auto focus the
   * searched element if it's not the first keyboard displayed.
   */
  test('deep linkng mixin does not focus on second element', async () => {
    const topRowAreFunctionKeysToggle = subsection.shadowRoot!.querySelector(
        '#externalTopRowAreFunctionKeysButton');
    assert(topRowAreFunctionKeysToggle);
    subsection.set('keyboardIndex', 1);
    // Enter the page from auto repeat search tag.
    const url = new URLSearchParams(
        'search=keyboard&settingId=' +
        encodeURIComponent(KEYBOARD_FUNCTION_KEYS_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_KEYBOARD,
        /* dynamicParams= */ url, /* removeSearch= */ true);
    await flushTasks();

    assertEquals(null, subsection.shadowRoot!.activeElement);

    const switchTopRowKeysButton = subsection.shadowRoot!.querySelector(
        '#blockMetaFunctionKeyRewritesButton');
    assert(switchTopRowKeysButton);

    const searchSwitchTopRowKeysUrl = new URLSearchParams(
        'search=switch+top&settingId=' +
        encodeURIComponent(KEYBOARD_SWITCH_TOP_ROW_KEYS_SETTING_ID));

    await Router.getInstance().navigateTo(
        routes.PER_DEVICE_KEYBOARD,
        /* dynamicParams= */ searchSwitchTopRowKeysUrl,
        /* removeSearch= */ true);

    await flushTasks();
    assertEquals(null, subsection.shadowRoot!.activeElement);
  });

  /**
   * Verifies that the indicator policy is properly reflected in the UI.
   */
  test('top row are fkeys policy reflected in UI', async () => {
    subsection.set('keyboardPolicies', {
      topRowAreFkeysPolicy:
          {policy_status: PolicyStatus.kManaged, value: false},
    });
    await flushTasks();
    const topRowAreFunctionKeysToggle = subsection.shadowRoot!.querySelector(
        '#externalTopRowAreFunctionKeysButton');
    assert(topRowAreFunctionKeysToggle);
  });

  test(
      'Built-in Keyboard customize keys rows has additional class name',
      async () => {
        await changeIsExternalState(false);
        assertTrue(subsection.shadowRoot!.querySelector('#remapKeyboardKeys')!
                       .classList.contains('remap-keyboard-keys-row-internal'));
      });

  test(
      'Verify keyboard backlight control elements visibility with flag',
      async () => {
        setKeyboardBacklightControlEnabled(true);
        await changeIsExternalState(false);

        // Initially, elements should be visible.
        assertTrue(isVisible(getElement('#rgbKeyboardControlLink')));
        assertTrue(isVisible(getElement('#keyboardAutoBrightnessToggle')));
        assertTrue(isVisible(getElement('#keyboardBrightnessSlider')));

        // Disable keyboard backlight control flag and reinitialize.
        setKeyboardBacklightControlEnabled(false);
        await initializePerDeviceKeyboardSubsection(
            fakeKeyboards, /*rgbKeyboardSupported=*/ true,
            /*hasKeyboardBacklight=*/ true,
            /*hasAmbientLightSensor=*/ true);
        await changeIsExternalState(false);

        // Elements should be hidden after flag is disabled.
        assertFalse(isVisible(getElement('#rgbKeyboardControlLink')));
        assertFalse(isVisible(getElement('#keyboardAutoBrightnessToggle')));
        assertFalse(isVisible(getElement('#keyboardBrightnessSlider')));
      });

  test(
      'Verify elements visibility with keyboard backlight status', async () => {
        setKeyboardBacklightControlEnabled(true);
        await initializePerDeviceKeyboardSubsection(
            fakeKeyboards, /*rgbKeyboardSupported=*/ true,
            /*hasKeyboardBacklight=*/ true,
            /*hasAmbientLightSensor=*/ true);
        await changeIsExternalState(false);
        assertTrue(isVisible(getElement('#keyboardAutoBrightnessToggle')));
        assertTrue(isVisible(getElement('#keyboardBrightnessSlider')));

        // Disable keyboard backlight, then reinitialize.
        await initializePerDeviceKeyboardSubsection(
            fakeKeyboards, /*rgbKeyboardSupported=*/ true,
            /*hasKeyboardBacklight=*/ false,
            /*hasAmbientLightSensor=*/ true);
        await changeIsExternalState(false);
        assertFalse(isVisible(getElement('#keyboardAutoBrightnessToggle')));
        assertFalse(isVisible(getElement('#keyboardBrightnessSlider')));
      });

  test('Verify keyboard auto brightness toggle visibility', async () => {
    setKeyboardBacklightControlEnabled(true);
    await initializePerDeviceKeyboardSubsection(
        fakeKeyboards, /*rgbKeyboardSupported=*/ true,
        /*hasKeyboardBacklight=*/ true,
        /*hasAmbientLightSensor=*/ true);
    await changeIsExternalState(false);
    assertTrue(isVisible(getElement('#keyboardAutoBrightnessToggle')));

    // Disable ambient light sensor, then reinitialize.
    await initializePerDeviceKeyboardSubsection(
        fakeKeyboards, /*rgbKeyboardSupported=*/ false,
        /*hasKeyboardBacklight=*/ true,
        /*hasAmbientLightSensor=*/ false);
    await changeIsExternalState(false);
    assertFalse(isVisible(getElement('#keyboardAutoBrightnessToggle')));
  });

  test('Verify rgb keyboard control link visiblity', async () => {
    setKeyboardBacklightControlEnabled(true);
    await initializePerDeviceKeyboardSubsection(
        fakeKeyboards, /*rgbKeyboardSupported=*/ true,
        /*hasKeyboardBacklight=*/ true, /*hasAmbientLightSensor=*/ true);
    await changeIsExternalState(false);
    assertTrue(isVisible(getElement('#rgbKeyboardControlLink')));

    // Disable RGB keyboard support, then reinitialize.
    await initializePerDeviceKeyboardSubsection(
        fakeKeyboards, /*rgbKeyboardSupported=*/ false,
        /*hasKeyboardBacklight=*/ true,
        /*hasAmbientLightSensor=*/ true);
    await changeIsExternalState(false);
    assertFalse(isVisible(getElement('#rgbKeyboardControlLink')));
  });

  test('observe keyboard brightness change', async () => {
    await changeIsExternalState(false);

    const slider = subsection.shadowRoot!.querySelector<SettingsSliderElement>(
        '#keyboardBrightnessSlider');
    assertTrue(!!slider);

    // Define initial and test values for keyboard brightness to improve
    // readability.
    const initialBrightness = 40.0;
    const firstAdjustedBrightness = 60.5;
    const secondAdjustedBrightness = 20.5;

    // Verify initial brightness is set correctly when observer is registered.
    assertEquals(initialBrightness, slider.pref!.value);

    // Simulate a keyboard brightness change and verify the slider updates
    // accordingly.
    provider.sendKeyboardBrightnessChange(firstAdjustedBrightness);
    await flushTasks();
    assertEquals(firstAdjustedBrightness, slider.pref!.value);

    // Simulate another keyboard brightness change.
    provider.sendKeyboardBrightnessChange(secondAdjustedBrightness);
    await flushTasks();
    assertEquals(secondAdjustedBrightness, slider.pref!.value);
  });

  test('observe keyboard ambient light sensor enabled change', async () => {
    await changeIsExternalState(false);
    const toggle =
        subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#keyboardAutoBrightnessToggle');
    assertTrue(!!toggle);

    // Simulate a keyboard ambient light sensor enabled change and verify the
    // toggle updates accordingly.
    provider.sendKeyboardAmbientLightSensorEnabledChange(true);
    await flushTasks();
    assertTrue(toggle.pref!.value);

    // Simulate another keyboard ambient light sensor enabled change.
    provider.sendKeyboardAmbientLightSensorEnabledChange(false);
    await flushTasks();
    assertFalse(toggle.pref!.value);
  });

  test('Set keyboard brightness via slider', async () => {
    await changeIsExternalState(false);
    const slider = subsection.shadowRoot!.querySelector<SettingsSliderElement>(
        '#keyboardBrightnessSlider');
    assertTrue(!!slider);

    const initialBrightness = 40;
    const firstAdjustedBrightness = 60.5;
    const secondAdjustedBrightness = 20.5;

    // Default brightness is 40.
    assertEquals(initialBrightness, slider.pref.value);
    assertEquals(initialBrightness, provider.getKeyboardBrightness());

    // Set keyboard brightness via slider.
    slider.pref.value = firstAdjustedBrightness;
    slider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
    await flushTasks();
    assertEquals(firstAdjustedBrightness, provider.getKeyboardBrightness());

    // Set keyboard brightness via slider again.
    slider.pref.value = secondAdjustedBrightness;
    slider.dispatchEvent(new CustomEvent('cr-slider-value-changed'));
    await flushTasks();
    assertEquals(secondAdjustedBrightness, provider.getKeyboardBrightness());
  });

  test(
      'Set keyboard ambient light sensor enable via toggle element',
      async () => {
        await changeIsExternalState(false);
        const toggle =
            subsection.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#keyboardAutoBrightnessToggle');
        assertTrue(!!toggle);

        // Verify initial state of the toggle and fake provider is 'false'.
        assertFalse(toggle.checked);
        assertFalse(provider.getKeyboardAmbientLightSensorEnabled());

        // Toggle to enable the keyboard ambient light sensor.
        toggle.click();
        await flushTasks();
        assertTrue(toggle.checked);
        assertTrue(provider.getKeyboardAmbientLightSensorEnabled());

        // Toggle again to disable the sensor.
        toggle.click();
        await flushTasks();
        assertFalse(toggle.checked);
        assertFalse(provider.getKeyboardAmbientLightSensorEnabled());
      });

  test('Record keyboard color link clicked', async () => {
    await changeIsExternalState(false);
    assertEquals(0, provider.getKeyboardColorLinkClicks());
    const rgbKeyboardControlLink =
        subsection.shadowRoot!.querySelector<CrLinkRowElement>(
            '#rgbKeyboardControlLink');
    assertTrue(!!rgbKeyboardControlLink);

    rgbKeyboardControlLink.click();
    await flushTasks();
    assertEquals(1, provider.getKeyboardColorLinkClicks());
  });

  test('Record brightness change from slider', async () => {
    await changeIsExternalState(false);
    const slider = subsection.shadowRoot!.querySelector<SettingsSliderElement>(
        '#keyboardBrightnessSlider');
    assertTrue(!!slider);
    assertEquals(
        0, provider.getRecordKeyboardBrightnessChangeFromSliderCallCount());

    // Ensure no metric is recorded on pointerdown.
    slider.dispatchEvent(new PointerEvent('pointerdown'));
    assertEquals(
        0, provider.getRecordKeyboardBrightnessChangeFromSliderCallCount());

    // Verify metric is recorded once on pointerup.
    slider.dispatchEvent(new PointerEvent('pointerup'));
    assertEquals(
        1, provider.getRecordKeyboardBrightnessChangeFromSliderCallCount());

    // Keydown events shouldn't trigger metric recording.
    slider.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowLeft'}));
    assertEquals(
        1, provider.getRecordKeyboardBrightnessChangeFromSliderCallCount());

    // Verify metric records on keyup with arrow keys.
    slider.dispatchEvent(new KeyboardEvent('keyup', {key: 'ArrowLeft'}));
    assertEquals(
        2, provider.getRecordKeyboardBrightnessChangeFromSliderCallCount());

    // Non-arrow keyup events shouldn't record metrics.
    slider.dispatchEvent(new KeyboardEvent('keyup', {key: 'Enter'}));
    assertEquals(
        2, provider.getRecordKeyboardBrightnessChangeFromSliderCallCount());
  });

  test('Observe lid state change for internal keyboard', async () => {
    // Set keyboard to internal state.
    await changeIsExternalState(false);
    let subsectionHeader =
        subsection.shadowRoot!.querySelector<HTMLElement>('#subsectionHeader');
    let subsectionBody =
        subsection.shadowRoot!.querySelector<HTMLElement>('.subsection');

    // Subsection header and body should be present.
    assertTrue(!!subsectionHeader);
    assertTrue(!!subsectionBody);

    // Simulate lid close.
    provider.setLidStateClosed();
    await flushTasks();

    // Subsection header and body should be hidden.
    subsectionHeader =
        subsection.shadowRoot!.querySelector<HTMLElement>('#subsectionHeader');
    subsectionBody =
        subsection.shadowRoot!.querySelector<HTMLElement>('.subsection');
    assertFalse(!!subsectionHeader);
    assertFalse(!!subsectionBody);

    // Simulate lid open.
    provider.setLidStateOpen();
    await flushTasks();

    // Subsection header and body should be visible again.
    subsectionHeader =
        subsection.shadowRoot!.querySelector<HTMLElement>('#subsectionHeader');
    subsectionBody =
        subsection.shadowRoot!.querySelector<HTMLElement>('.subsection');
    assertTrue(!!subsectionHeader);
    assertTrue(!!subsectionBody);
  });

  test('Observe lid state change for external keyboard', async () => {
    // Set keyboard to external state.
    await changeIsExternalState(true);

    // Subsection header and body should be present.
    let subsectionHeader =
        subsection.shadowRoot!.querySelector<HTMLElement>('#subsectionHeader');
    let subsectionBody =
        subsection.shadowRoot!.querySelector<HTMLElement>('.subsection');
    assertTrue(!!subsectionHeader);
    assertTrue(!!subsectionBody);

    // Simulate lid close.
    provider.setLidStateClosed();
    await flushTasks();

    // Subsection header and body should still be present.
    subsectionHeader =
        subsection.shadowRoot!.querySelector<HTMLElement>('#subsectionHeader');
    subsectionBody =
        subsection.shadowRoot!.querySelector<HTMLElement>('.subsection');
    assertTrue(!!subsectionHeader);
    assertTrue(!!subsectionBody);
  });
});
