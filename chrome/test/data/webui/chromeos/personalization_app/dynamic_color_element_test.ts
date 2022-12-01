// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for dynamic-color-element component.  */

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {DynamicColorElement} from 'chrome://personalization/js/personalization_app.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {dispatchKeydown, getActiveElement, initElement, teardownElement, waitForActiveElement} from './personalization_app_test_utils.js';

suite('DynamicColorElementTest', function() {
  let dynamicColorElement: DynamicColorElement|null;

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

  function getColorSchemeButtons(): NodeListOf<Element> {
    return getColorSchemeSelector().querySelectorAll('cr-button')!;
  }

  function getStaticColorButtons(): NodeListOf<Element> {
    return getStaticColorSelector().querySelectorAll('cr-button')!;
  }

  setup(async () => {
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
    assertTrue(getToggleButton().checked, 'toggle button should be on');
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
    const toggleButton = getToggleButton();
    toggleButton!.click();
    assertFalse(
        toggleButton!.checked,
        'toggle button should be off to show the static color buttons');
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
});
