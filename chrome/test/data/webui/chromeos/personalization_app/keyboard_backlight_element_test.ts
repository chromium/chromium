// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {KeyboardBacklightActionName, KeyboardBacklightElement, KeyboardBacklightObserver, SetCurrentBacklightStateAction, SetShouldShowNudgeAction, SetWallpaperColorAction} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestKeyboardBacklightProvider} from './test_keyboard_backlight_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('KeyboardBacklightElementTest', function() {
  let keyboardBacklightElement: KeyboardBacklightElement|null;
  let keyboardBacklightProvider: TestKeyboardBacklightProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    keyboardBacklightProvider = mocks.keyboardBacklightProvider;
    personalizationStore = mocks.personalizationStore;
    KeyboardBacklightObserver.initKeyboardBacklightObserverIfNeeded();
  });

  teardown(async () => {
    await teardownElement(keyboardBacklightElement);
    keyboardBacklightElement = null;
    KeyboardBacklightObserver.shutdown();
  });


  test('displays content', async () => {
    keyboardBacklightElement = initElement(KeyboardBacklightElement);
    const labelContainer = keyboardBacklightElement.shadowRoot!.getElementById(
        'keyboardBacklightLabel');
    assertTrue(!!labelContainer, 'keyboard backlight label should be shown.');
    const text = labelContainer!.querySelector('p');
    assertTrue(!!text);
    assertEquals(
        keyboardBacklightElement.i18n('keyboardBacklightTitle'),
        text.textContent);
    const colorSelectorElement =
        keyboardBacklightElement.shadowRoot!.querySelector('color-selector') as
        HTMLElement;
    assertTrue(
        !!colorSelectorElement, 'color-selector element should be displayed.');
    const selectorContainer =
        colorSelectorElement.shadowRoot!.getElementById('selector');
    assertTrue(!!selectorContainer);
    const colorContainers = selectorContainer!.querySelectorAll('.selectable');
    assertEquals(9, colorContainers!.length);
  });

  test('sets backlight color when a color preset is clicked', async () => {
    keyboardBacklightElement = initElement(KeyboardBacklightElement);
    const colorSelectorElement =
        keyboardBacklightElement.shadowRoot!.querySelector('color-selector') as
        HTMLElement;
    assertTrue(
        !!colorSelectorElement, 'color-selector element should be displayed.');
    const selectorContainer =
        colorSelectorElement.shadowRoot!.getElementById('selector');
    assertTrue(!!selectorContainer);
    const colorContainers = selectorContainer!.querySelectorAll('.selectable');
    assertEquals(9, colorContainers!.length);

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE);
    (colorContainers[1] as HTMLElement).click();
    await keyboardBacklightProvider.whenCalled('setBacklightColor');

    const action =
        await personalizationStore.waitForAction(
            KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE) as
        SetCurrentBacklightStateAction;
    assertTrue(!!action.currentBacklightState);
    assertTrue(
        !!personalizationStore.data.keyboardBacklight.currentBacklightState);
  });

  test('sets backlight color in store on first load', async () => {
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE);
    keyboardBacklightElement = initElement(KeyboardBacklightElement);
    await keyboardBacklightProvider.whenCalled('setKeyboardBacklightObserver');
    keyboardBacklightProvider.fireOnBacklightStateChanged(
        keyboardBacklightProvider.currentBacklightState);
    const action =
        await personalizationStore.waitForAction(
            KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE) as
        SetCurrentBacklightStateAction;
    assertDeepEquals(
        keyboardBacklightProvider.currentBacklightState,
        action.currentBacklightState);
  });

  test('sets backlight color data in store on changed', async () => {
    await keyboardBacklightProvider.whenCalled('setKeyboardBacklightObserver');

    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE);
    keyboardBacklightProvider.keyboardBacklightObserverRemote!
        .onBacklightStateChanged(
            keyboardBacklightProvider.currentBacklightState);

    const {currentBacklightState} =
        await personalizationStore.waitForAction(
            KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE) as
        SetCurrentBacklightStateAction;
    assertDeepEquals(
        keyboardBacklightProvider.currentBacklightState, currentBacklightState);
  });

  test('sets wallpaper color in store on first load', async () => {
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_WALLPAPER_COLOR);
    keyboardBacklightElement = initElement(KeyboardBacklightElement);
    await keyboardBacklightProvider.whenCalled('setKeyboardBacklightObserver');
    const wallpaperColor = {value: 0x123456};
    keyboardBacklightProvider.fireOnWallpaperColorChanged(wallpaperColor);
    const action = await personalizationStore.waitForAction(
                       KeyboardBacklightActionName.SET_WALLPAPER_COLOR) as
        SetWallpaperColorAction;
    assertDeepEquals(wallpaperColor, action.wallpaperColor);
  });

  test('shows toast on load', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_SHOULD_SHOW_NUDGE);
    keyboardBacklightElement = initElement(KeyboardBacklightElement);
    const colorSelectorElement =
        keyboardBacklightElement.shadowRoot!.querySelector('color-selector') as
        HTMLElement;
    assertTrue(
        !!colorSelectorElement, 'color-selector element should be displayed.');
    const action = await personalizationStore.waitForAction(
                       KeyboardBacklightActionName.SET_SHOULD_SHOW_NUDGE) as
        SetShouldShowNudgeAction;
    assertTrue(action.shouldShowNudge);
    assertTrue(!!colorSelectorElement.shadowRoot!.querySelector('#toast'));
  });

  test('automatically dismisses toast after 3 seconds', async () => {
    // Spy on calls to |window.setTimeout|.
    const setTimeout = window.setTimeout;
    const setTimeoutCalls: Array<{handler: Function | string, delay?: number}> =
        [];
    window.setTimeout =
        (handler: Function|string, delay?: number, ...args: any[]): number => {
          setTimeoutCalls.push({handler, delay});
          return setTimeout(handler, delay, args);
        };

    keyboardBacklightElement = initElement(KeyboardBacklightElement);
    const colorSelectorElement =
        keyboardBacklightElement.shadowRoot!.querySelector('color-selector') as
        HTMLElement;
    assertTrue(
        !!colorSelectorElement, 'color-selector element should be displayed.');

    // Create and render the toast.
    personalizationStore.data.keyboardBacklight.shouldShowNudge = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(keyboardBacklightElement);

    // Expect that a timeout will have been scheduled for 3 seconds.
    const setTimeoutCall: {handler: Function|string, delay?: number}|undefined =
        setTimeoutCalls.find((setTimeoutCall) => {
          return typeof setTimeoutCall.handler === 'function' &&
              setTimeoutCall.delay === 3000;
        });
    assertNotEquals(setTimeoutCall, undefined);

    // Expect that the timeout will result in toast dismissal.
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_SHOULD_SHOW_NUDGE);
    (setTimeoutCall!.handler as Function)();
    const action = await personalizationStore.waitForAction(
                       KeyboardBacklightActionName.SET_SHOULD_SHOW_NUDGE) as
        SetShouldShowNudgeAction;
    assertFalse(action.shouldShowNudge);
  });

  test(
      'shows customization button if multi-zone rgb keyboard is supported',
      async () => {
        loadTimeData.overrideValues(
            {keyboardBacklightZoneCount: keyboardBacklightProvider.zoneCount});
        keyboardBacklightElement = initElement(KeyboardBacklightElement);
        const customizationButton =
            keyboardBacklightElement.shadowRoot!.getElementById(
                'zoneCustomizationButton');
        assertTrue(!!customizationButton);
      });

  test('clicking on customization button opens a dialog', async () => {
    loadTimeData.overrideValues(
        {keyboardBacklightZoneCount: keyboardBacklightProvider.zoneCount});
    keyboardBacklightElement = initElement(KeyboardBacklightElement);
    const customizationButton =
        keyboardBacklightElement.shadowRoot!.getElementById(
            'zoneCustomizationButton');
    assertTrue(!!customizationButton);
    assertEquals(
        null,
        keyboardBacklightElement.shadowRoot!.querySelector(
            'zone-customization'),
        'no dialog until button clicked');
    customizationButton.click();
    await waitAfterNextRender(keyboardBacklightElement);
    assertTrue(
        !!keyboardBacklightElement.shadowRoot!.querySelector(
            'zone-customization'),
        'dialog exists after button is clicked');
  });

  test('shows wallpaper color at the end with multi-zone enabled', async () => {
    loadTimeData.overrideValues(
        {keyboardBacklightZoneCount: keyboardBacklightProvider.zoneCount});

    keyboardBacklightElement = initElement(KeyboardBacklightElement);
    const colorSelectorElement =
        keyboardBacklightElement.shadowRoot!.querySelector('color-selector') as
        HTMLElement;
    assertTrue(
        !!colorSelectorElement, 'color-selector element should be displayed.');

    const selectorContainer =
        colorSelectorElement.shadowRoot!.getElementById('selector');
    assertTrue(!!selectorContainer);
    const colorContainers = selectorContainer!.querySelectorAll('.selectable');
    assertEquals(9, colorContainers!.length);
    const wallpaperColorIcon =
        colorContainers[8]!.querySelector('color-icon') as HTMLElement;
    assertEquals('Wallpaper color', wallpaperColorIcon.ariaLabel);
    assertEquals(0, selectorContainer!.querySelectorAll('.divider').length);
    assertTrue(!!colorSelectorElement?.shadowRoot!.getElementById(
        'wallpaperColorDescription'));
  });

  test(
      'shows wallpaper color button at the beginning with multi-zone disabled',
      async () => {
        loadTimeData.overrideValues({keyboardBacklightZoneCount: 0});

        keyboardBacklightElement = initElement(KeyboardBacklightElement);
        const colorSelectorElement =
            keyboardBacklightElement.shadowRoot!.querySelector(
                'color-selector') as HTMLElement;
        assertTrue(
            !!colorSelectorElement,
            'color-selector element should be displayed.');

        const selectorContainer =
            colorSelectorElement.shadowRoot!.getElementById('selector');
        assertTrue(!!selectorContainer);
        const colorContainers =
            selectorContainer!.querySelectorAll('.selectable');
        assertEquals(9, colorContainers!.length);
        const wallpaperColorIcon =
            colorContainers[0]!.querySelector('color-icon') as HTMLElement;
        assertEquals('Wallpaper color', wallpaperColorIcon.ariaLabel);
        assertEquals(1, selectorContainer!.querySelectorAll('.divider').length);
        assertFalse(!!keyboardBacklightElement?.shadowRoot!.getElementById(
            'wallpaperColorDescription'));
      });

  test('displays zone selector in customization dialog', async () => {
    loadTimeData.overrideValues(
        {keyboardBacklightZoneCount: keyboardBacklightProvider.zoneCount});
    keyboardBacklightElement = initElement(KeyboardBacklightElement);
    personalizationStore.notifyObservers();
    const customizationButton =
        keyboardBacklightElement.shadowRoot!.getElementById(
            'zoneCustomizationButton');
    assertTrue(!!customizationButton);
    customizationButton!.click();
    await waitAfterNextRender(keyboardBacklightElement);
    const zoneCustomizationElement =
        keyboardBacklightElement.shadowRoot!.querySelector(
            'zone-customization');
    assertTrue(!!zoneCustomizationElement);
    const zoneTabs =
        zoneCustomizationElement.shadowRoot!.querySelectorAll('.zone-tab');
    assertEquals(
        5, zoneTabs!.length, '5 zones should display in customization dialog');
  });
});
