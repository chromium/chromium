// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {KeyboardBacklight, KeyboardBacklightActionName, KeyboardBacklightObserver, SetBacklightColorAction, SetShouldShowNudgeAction, SetWallpaperColorAction} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestKeyboardBacklightProvider} from './test_keyboard_backlight_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('KeyboardBacklightTest', function() {
  let keyboardBacklightElement: KeyboardBacklight|null;
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
    keyboardBacklightElement = initElement(KeyboardBacklight);
    const labelContainer = keyboardBacklightElement.shadowRoot!.getElementById(
        'keyboardBacklightLabel');
    assertTrue(!!labelContainer);
    const text = labelContainer.querySelector('p');
    assertTrue(!!text);
    assertEquals(
        keyboardBacklightElement.i18n('keyboardBacklightTitle'),
        text.textContent);

    const selectorContainer =
        keyboardBacklightElement.shadowRoot!.getElementById('selector');
    assertTrue(!!selectorContainer);
    const colorContainers =
        selectorContainer.querySelectorAll('.color-container');
    assertEquals(9, colorContainers!.length);
  });

  test('sets backlight color when a color preset is clicked', async () => {
    keyboardBacklightElement = initElement(KeyboardBacklight);
    const selectorContainer =
        keyboardBacklightElement.shadowRoot!.getElementById('selector');
    assertTrue(!!selectorContainer);
    const colorContainers =
        selectorContainer.querySelectorAll('.color-container');
    assertEquals(9, colorContainers!.length);

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_BACKLIGHT_COLOR);
    (colorContainers[1] as HTMLElement).click();
    await keyboardBacklightProvider.whenCalled('setBacklightColor');

    const action = await personalizationStore.waitForAction(
                       KeyboardBacklightActionName.SET_BACKLIGHT_COLOR) as
        SetBacklightColorAction;
    assertTrue(!!action.backlightColor);
    assertTrue(!!personalizationStore.data.keyboardBacklight.backlightColor);
  });

  test('sets backlight color in store on first load', async () => {
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_BACKLIGHT_COLOR);
    keyboardBacklightElement = initElement(KeyboardBacklight);
    await keyboardBacklightProvider.whenCalled('setKeyboardBacklightObserver');
    keyboardBacklightProvider.fireOnBacklightColorChanged(
        keyboardBacklightProvider.backlightColor);
    const action = await personalizationStore.waitForAction(
                       KeyboardBacklightActionName.SET_BACKLIGHT_COLOR) as
        SetBacklightColorAction;
    assertEquals(
        keyboardBacklightProvider.backlightColor, action.backlightColor);
  });

  test('sets backlight color data in store on changed', async () => {
    await keyboardBacklightProvider.whenCalled('setKeyboardBacklightObserver');

    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_BACKLIGHT_COLOR);
    keyboardBacklightProvider.keyboardBacklightObserverRemote!
        .onBacklightColorChanged(keyboardBacklightProvider.backlightColor);

    const {backlightColor} =
        await personalizationStore.waitForAction(
            KeyboardBacklightActionName.SET_BACKLIGHT_COLOR) as
        SetBacklightColorAction;
    assertEquals(keyboardBacklightProvider.backlightColor, backlightColor);
  });

  test('sets wallpaper color in store on first load', async () => {
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_WALLPAPER_COLOR);
    keyboardBacklightElement = initElement(KeyboardBacklight);
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
    keyboardBacklightElement = initElement(KeyboardBacklight);
    const action = await personalizationStore.waitForAction(
                       KeyboardBacklightActionName.SET_SHOULD_SHOW_NUDGE) as
        SetShouldShowNudgeAction;
    assertTrue(action.shouldShowNudge);
    assertTrue(!!keyboardBacklightElement.shadowRoot!.querySelector('#toast'));
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

    keyboardBacklightElement = initElement(KeyboardBacklight);
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
        loadTimeData.overrideValues({isMultiZoneRgbKeyboardSupported: true});
        keyboardBacklightElement = initElement(KeyboardBacklight);
        const customizationButton =
            keyboardBacklightElement.shadowRoot!.getElementById(
                'zoneCustomizationButton');
        assertTrue(!!customizationButton);
      });

  test('clicking on customization button opens a dialog', async () => {
    loadTimeData.overrideValues({isMultiZoneRgbKeyboardSupported: true});
    keyboardBacklightElement = initElement(KeyboardBacklight);
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
});
