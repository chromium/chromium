// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {BacklightColor, CurrentBacklightState, KeyboardBacklightActionName, KeyboardBacklightObserver, SetCurrentBacklightStateAction, staticColorIds, ZoneCustomizationElement} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestKeyboardBacklightProvider} from './test_keyboard_backlight_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('ZoneCustomizationElementTest', function() {
  let zoneCustomizationElement: ZoneCustomizationElement|null;
  let keyboardBacklightProvider: TestKeyboardBacklightProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    keyboardBacklightProvider = mocks.keyboardBacklightProvider;
    personalizationStore = mocks.personalizationStore;
    KeyboardBacklightObserver.initKeyboardBacklightObserverIfNeeded();
  });

  teardown(async () => {
    await teardownElement(zoneCustomizationElement);
    zoneCustomizationElement = null;
    KeyboardBacklightObserver.shutdown();
  });

  async function initZoneCustomizationElement() {
    loadTimeData.overrideValues(
        {keyboardBacklightZoneCount: keyboardBacklightProvider.zoneCount});
    personalizationStore.data.keyboardBacklight.currentBacklightState =
        keyboardBacklightProvider.currentBacklightState;
    personalizationStore.notifyObservers();
    zoneCustomizationElement = initElement(ZoneCustomizationElement);
    await waitAfterNextRender(zoneCustomizationElement);
  }

  function verifyColorIconAriaChecked(
      expectedColor: string, colorContainers: NodeListOf<Element>) {
    for (let i = 0; i < colorContainers!.length; i++) {
      const colorContainer = colorContainers[i] as HTMLElement;
      const colorIconElem =
          colorContainer!.querySelector('color-icon') as HTMLElement;
      const colorId = colorContainer.id;
      if (colorId === expectedColor) {
        assertEquals(
            'true', colorIconElem.ariaChecked,
            `${expectedColor} should be highlighted.`);
      } else {
        assertEquals(
            'false', colorIconElem.ariaChecked,
            `${colorId} should not be highlighted.`);
      }
    }
  }

  function verifyNoColorIconsAriaChecked(colorContainers: NodeListOf<Element>) {
    for (let i = 0; i < colorContainers!.length; i++) {
      const colorContainer = colorContainers[i] as HTMLElement;
      const colorIconElem =
          colorContainer!.querySelector('color-icon') as HTMLElement;
      const colorId = colorContainer.id;
      assertNotEquals(
          'rainbowColor', colorId,
          'No rainbow color option should be available');
      assertEquals(
          'false', colorIconElem.ariaChecked,
          `${colorId} should not be highlighted.`);
    }
  }

  test(
      'displays content with current backlight state as a static color',
      async () => {
        await initZoneCustomizationElement();
        const zoneSelector =
            zoneCustomizationElement!.shadowRoot!.getElementById(
                'zoneSelector');
        assertTrue(!!zoneSelector, 'zone selector should display');
        const zoneTabs =
            zoneCustomizationElement!.shadowRoot!.querySelectorAll('.zone-tab');
        assertEquals(
            5, zoneTabs!.length,
            '5 zones should display in customization dialog');
        const colorSelectorElement =
            zoneCustomizationElement!.shadowRoot!.querySelector(
                'color-selector');
        assertTrue(!!colorSelectorElement);
        const colorContainers =
            colorSelectorElement.shadowRoot!.querySelectorAll('.selectable');
        assertEquals(
            8, colorContainers!.length,
            '8 color options should display in customization dialog');
        const dialogCloseButton =
            zoneCustomizationElement!.shadowRoot!.getElementById(
                'dialogCloseButton');
        assertTrue(!!dialogCloseButton, 'close dialog button should display');
      });

  test(
      'updates zone content with current backlight state as zone colors',
      async () => {
        keyboardBacklightProvider.setZoneCount(4);
        keyboardBacklightProvider.setCurrentBacklightState(
            {zoneColors: keyboardBacklightProvider.zoneColors});
        await initZoneCustomizationElement();
        const zoneSelector =
            zoneCustomizationElement!.shadowRoot!.getElementById(
                'zoneSelector');
        assertTrue(!!zoneSelector, 'zone selector should display');
        const zoneTabs =
            zoneCustomizationElement!.shadowRoot!.querySelectorAll('.zone-tab');
        assertEquals(
            4, zoneTabs!.length,
            '4 zones should display in customization dialog');
        const colorIcons =
            zoneCustomizationElement!.shadowRoot!.querySelectorAll(
                'color-icon');
        assertEquals(
            4, colorIcons!.length,
            '4 color icons should display in customization dialog');
        // Color of the color-icon displayed in each zone should match with the
        // corresponding one in zone colors.
        for (let i = 0; i < 4; i++) {
          const zoneColor = keyboardBacklightProvider.zoneColors[i];
          const expectedColorId = staticColorIds[zoneColor!];
          const colorId =
              (colorIcons[i] as HTMLElement).getAttribute('color-id');
          assertEquals(
              expectedColorId, colorId,
              `colorId for zone ${i + 1} should be ${expectedColorId}`);
        }
      });

  test('sets zone colors data in store on first load', async () => {
    const currentBacklightState: CurrentBacklightState = {
      zoneColors: keyboardBacklightProvider.zoneColors,
    };
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE);
    await keyboardBacklightProvider.whenCalled('setKeyboardBacklightObserver');
    keyboardBacklightProvider.fireOnBacklightStateChanged(
        currentBacklightState);
    const action =
        await personalizationStore.waitForAction(
            KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE) as
        SetCurrentBacklightStateAction;
    assertDeepEquals(currentBacklightState, action.currentBacklightState);
  });

  test('displays correct zone color when a zone is selected', async () => {
    keyboardBacklightProvider.setZoneCount(4);
    keyboardBacklightProvider.setCurrentBacklightState(
        {zoneColors: keyboardBacklightProvider.zoneColors});
    await initZoneCustomizationElement();
    const zoneSelector =
        zoneCustomizationElement!.shadowRoot!.getElementById('zoneSelector');
    assertTrue(!!zoneSelector, 'zone selector should display');
    const zoneTabs =
        zoneCustomizationElement!.shadowRoot!.querySelectorAll('.zone-tab');
    assertEquals(
        4, zoneTabs!.length, '4 zones should display in customization dialog');
    // Zone 2 has zone color as red, expect red color button to be highlighted.
    (zoneTabs[1] as CrButtonElement).click();
    const colorSelectorElement =
        zoneCustomizationElement!.shadowRoot!.querySelector('color-selector');
    assertTrue(!!colorSelectorElement, 'color-selector should display.');
    const colorContainers =
        colorSelectorElement.shadowRoot!.querySelectorAll('.selectable');
    assertEquals(8, colorContainers!.length);
    verifyColorIconAriaChecked('redColor', colorContainers);

    // Zone 4 has zone color as yellow, expect yellow color button to be
    // highlighted.
    (zoneTabs[3] as HTMLDivElement).click();
    await waitAfterNextRender(zoneCustomizationElement!);
    verifyColorIconAriaChecked('yellowColor', colorContainers);
  });

  test('sets color for a zone', async () => {
    keyboardBacklightProvider.setZoneCount(4);
    keyboardBacklightProvider.setCurrentBacklightState(
        {zoneColors: keyboardBacklightProvider.zoneColors});
    await initZoneCustomizationElement();
    const zoneSelector =
        zoneCustomizationElement!.shadowRoot!.getElementById('zoneSelector');
    assertTrue(!!zoneSelector, 'zone selector should display');
    const zoneTabs =
        zoneCustomizationElement!.shadowRoot!.querySelectorAll('.zone-tab');
    assertEquals(
        4, zoneTabs!.length, '4 zones should display in customization dialog');

    // Click on zone 2, expect red color icon to be highlighted.
    (zoneTabs[1] as HTMLDivElement).click();
    const colorSelectorElement =
        zoneCustomizationElement!.shadowRoot!.querySelector('color-selector') as
        HTMLElement;
    assertTrue(!!colorSelectorElement, 'color-selector should display.');
    const colorContainers =
        colorSelectorElement.shadowRoot!.querySelectorAll('.selectable');
    assertEquals(
        8, colorContainers.length!, 'there should be 8 color containers');
    verifyColorIconAriaChecked('redColor', colorContainers);

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE);

    // Selects wallpaper color, color of zone 2 should change to wallpaper.
    (colorContainers[7]!.querySelector('color-icon') as HTMLElement).click();

    await keyboardBacklightProvider.whenCalled('setBacklightZoneColor');
    const action =
        await personalizationStore.waitForAction(
            KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE) as
        SetCurrentBacklightStateAction;
    assertTrue(!!action.currentBacklightState);
    const expectedZoneColors = [...keyboardBacklightProvider.zoneColors];
    expectedZoneColors[1] = BacklightColor.kWallpaper;
    assertDeepEquals(
        expectedZoneColors, action.currentBacklightState.zoneColors);
    await waitAfterNextRender(zoneCustomizationElement!);
    verifyColorIconAriaChecked('wallpaperColor', colorContainers);
  });

  test('sets color for a zone that was preset rainbow', async () => {
    // When setting a color to a zone that rainbow color was selected as
    // backlight color earlier, the selected zone changes to the new color and
    // other zones change to white color.
    keyboardBacklightProvider.setZoneCount(4);
    keyboardBacklightProvider.setCurrentBacklightState({
      zoneColors: Array(4).fill(BacklightColor.kRainbow),
    });
    await initZoneCustomizationElement();
    const zoneSelector =
        zoneCustomizationElement!.shadowRoot!.getElementById('zoneSelector');
    assertTrue(!!zoneSelector, 'zone selector should display');
    const zoneTabs =
        zoneCustomizationElement!.shadowRoot!.querySelectorAll('.zone-tab');
    assertEquals(
        4, zoneTabs!.length, '4 zones should display in customization dialog');

    // Click on zone 2, none of color icons to be highlighted as no rainbow
    // color available in color options.
    (zoneTabs[1] as HTMLDivElement).click();
    const colorSelectorElement =
        zoneCustomizationElement!.shadowRoot!.querySelector('color-selector') as
        HTMLElement;
    assertTrue(!!colorSelectorElement, 'color-selector should display.');
    const colorContainers =
        colorSelectorElement.shadowRoot!.querySelectorAll('.selectable');
    assertEquals(
        8, colorContainers.length!, 'there should be 8 color containers');
    verifyNoColorIconsAriaChecked(colorContainers);

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE);

    // Selects wallpaper color, color of zone 2 should change to wallpaper.
    (colorContainers[7]!.querySelector('color-icon') as HTMLElement).click();

    await keyboardBacklightProvider.whenCalled('setBacklightZoneColor');
    assertDeepEquals(
        keyboardBacklightProvider.getCallCount('setBacklightZoneColor'), 4);

    const action =
        await personalizationStore.waitForAction(
            KeyboardBacklightActionName.SET_CURRENT_BACKLIGHT_STATE) as
        SetCurrentBacklightStateAction;
    assertTrue(!!action.currentBacklightState);
    const expectedZoneColors = Array(4).fill(BacklightColor.kWhite);
    expectedZoneColors[1] = BacklightColor.kWallpaper;
    assertDeepEquals(
        expectedZoneColors, action.currentBacklightState.zoneColors);
    await waitAfterNextRender(zoneCustomizationElement!);
    verifyColorIconAriaChecked('wallpaperColor', colorContainers);
  });
});
