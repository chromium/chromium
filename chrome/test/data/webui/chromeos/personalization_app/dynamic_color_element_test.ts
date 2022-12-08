// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for dynamic-color-element component.  */

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {DynamicColorElement, SetColorSchemePrefAction, SetStaticColorPrefAction, ThemeActionName} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, dispatchKeydown, getActiveElement, initElement, teardownElement, waitForActiveElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestThemeProvider} from './test_theme_interface_provider.js';

suite('DynamicColorElementTest', function() {
  let dynamicColorElement: DynamicColorElement|null;
  let personalizationStore: TestPersonalizationStore;
  let themeProvider: TestThemeProvider;

  function getToggleButton(): CrToggleElement {
    return dynamicColorElement!.shadowRoot!.querySelector('cr-toggle')!;
  }

  function getColorSchemeSelector(): HTMLElement {
    return dynamicColorElement!.shadowRoot!.getElementById(
        'colorSchemeSelector')!;
  }

  function getStaticColorSelector(): HTMLElement {
    return dynamicColorElement!.shadowRoot!.getElementById(
        'staticColorSelector')!;
  }

  function getColorSchemeButtons(): NodeListOf<CrButtonElement> {
    return getColorSchemeSelector().querySelectorAll('cr-button')!;
  }

  function getStaticColorButtons(): NodeListOf<CrButtonElement> {
    return getStaticColorSelector().querySelectorAll('cr-button')!;
  }

  function showStaticColorButtons() {
    const toggleButton = getToggleButton();
    if (toggleButton.checked) {
      toggleButton.click();
    }
    assertFalse(getStaticColorSelector().hidden);
  }

  function showColorSchemeButtons() {
    const toggleButton = getToggleButton();
    if (!toggleButton.checked) {
      toggleButton.click();
    }
    assertFalse(getColorSchemeSelector().hidden);
  }

  setup(async () => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    themeProvider = mocks.themeProvider;

    dynamicColorElement = initElement(DynamicColorElement)!;
    await waitAfterNextRender(dynamicColorElement);
  });

  teardown(async () => {
    teardownElement(dynamicColorElement);
  });

  test('displays content', async () => {
    assertEquals(
        '[temp]Theme color[temp]Auto',
        dynamicColorElement!.shadowRoot!.getElementById(
                                            'themeHeader')!.textContent);
    assertTrue(getToggleButton().checked, 'default toggle should be on');
    assertFalse(
        getColorSchemeSelector().hidden,
        'when the toggle is on, the color scheme buttons should be visible.');
    assertTrue(
        getStaticColorSelector().hidden,
        'when the toggle is on, the static color buttons should be hidden.');
  });

  test('flips toggle', async () => {
    const toggleButton = getToggleButton();
    const colorSchemeSelector = getColorSchemeSelector();
    const staticColorSelector = getStaticColorSelector();

    toggleButton.click();

    assertFalse(
        toggleButton.checked, 'after clicking toggle, toggle should be off');
    assertTrue(
        colorSchemeSelector.hidden,
        'when the toggle is off, the color scheme buttons should be hidden');
    assertFalse(
        staticColorSelector.hidden,
        'when the toggle is off, the static color buttons should be visible.');

    toggleButton.click();

    assertFalse(
        colorSchemeSelector.hidden,
        'when the toggle is on, the color scheme buttons should be visible.');
    assertTrue(
        staticColorSelector.hidden,
        'when the toggle is on, the static color buttons should be hidden.');
  });

  test('keypress navigates color scheme buttons', async () => {
    assertTrue(!!dynamicColorElement);
    showColorSchemeButtons();
    const colorSchemeButtons = getColorSchemeButtons();
    (colorSchemeButtons[0] as HTMLElement)!.focus();

    for (let i = 1; i <= 3; ++i) {
      dispatchKeydown(getActiveElement(dynamicColorElement), 'ArrowRight');
      await waitForActiveElement(colorSchemeButtons[i]!, dynamicColorElement!);
      assertEquals(0, getActiveElement(dynamicColorElement).tabIndex);
      assertEquals(
          'iron-selected', getActiveElement(dynamicColorElement).className);
    }

    for (let i = 2; i >= 0; --i) {
      dispatchKeydown(getActiveElement(dynamicColorElement), 'ArrowLeft');
      await waitForActiveElement(colorSchemeButtons[i]!, dynamicColorElement!);
      assertEquals(0, getActiveElement(dynamicColorElement).tabIndex);
      assertEquals(
          'iron-selected', getActiveElement(dynamicColorElement).className);
    }
  });

  test('keypress navigates static color buttons', async () => {
    assertTrue(!!dynamicColorElement);
    showStaticColorButtons();
    const staticColorButtons = getStaticColorButtons();
    (staticColorButtons![0] as HTMLElement)!.focus();

    for (let i = 1; i <= 3; ++i) {
      dispatchKeydown(getActiveElement(dynamicColorElement), 'ArrowRight');
      await waitForActiveElement(staticColorButtons[i]!, dynamicColorElement);
      assertEquals(0, getActiveElement(dynamicColorElement).tabIndex);
      assertEquals(
          'iron-selected', getActiveElement(dynamicColorElement).className);
    }

    for (let i = 2; i >= 0; --i) {
      dispatchKeydown(
          (getActiveElement(dynamicColorElement) as HTMLElement), 'ArrowLeft');
      await waitForActiveElement(staticColorButtons[i]!, dynamicColorElement);
      assertEquals(0, getActiveElement(dynamicColorElement).tabIndex);
      assertEquals(
          'iron-selected', getActiveElement(dynamicColorElement).className);
    }
  });

  test('sets color scheme', async () => {
    personalizationStore.expectAction(ThemeActionName.SET_COLOR_SCHEME);
    showColorSchemeButtons();

    getColorSchemeButtons()[1]!.click();
    await themeProvider.whenCalled('setColorScheme');

    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_COLOR_SCHEME) as SetColorSchemePrefAction;
    assertTrue(!!action.colorScheme);
  });

  test('set static color', async () => {
    personalizationStore.expectAction(ThemeActionName.SET_STATIC_COLOR);
    showStaticColorButtons();

    getStaticColorButtons()[1]!.click();
    await themeProvider.whenCalled('setStaticColor');

    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_STATIC_COLOR) as SetStaticColorPrefAction;
    assertTrue(!!action.staticColor);
  });
});
