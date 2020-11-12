// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/color_mode_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getColorModeString} from 'chrome://scanning/scanning_app_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {assertOrderedAlphabetically} from './scanning_app_test_utils.js';

const ColorMode = {
  BLACK_AND_WHITE: chromeos.scanning.mojom.ColorMode.kBlackAndWhite,
  GRAYSCALE: chromeos.scanning.mojom.ColorMode.kGrayscale,
  COLOR: chromeos.scanning.mojom.ColorMode.kColor,
};

export function colorModeSelectTest() {
  /** @type {?ColorModeSelectElement} */
  let colorModeSelect = null;

  setup(() => {
    colorModeSelect = /** @type {!ColorModeSelectElement} */ (
        document.createElement('color-mode-select'));
    assertTrue(!!colorModeSelect);
    document.body.appendChild(colorModeSelect);
  });

  teardown(() => {
    colorModeSelect.remove();
    colorModeSelect = null;
  });

  test('initializeColorModeSelect', () => {
    // Before options are added, the dropdown should be disabled and empty.
    const select = colorModeSelect.$$('select');
    assertTrue(!!select);
    assertTrue(select.disabled);
    assertEquals(0, select.length);

    const firstColorMode = ColorMode.COLOR;
    const secondColorMode = ColorMode.GRAYSCALE;
    colorModeSelect.colorModes = [firstColorMode, secondColorMode];
    flush();

    // Verify that adding more than one color mode results in the dropdown
    // becoming enabled with the correct options.
    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(
        getColorModeString(firstColorMode),
        select.options[0].textContent.trim());
    assertEquals(
        getColorModeString(secondColorMode),
        select.options[1].textContent.trim());
    assertEquals(firstColorMode.toString(), select.value);
  });

  test('colorModeSelectDisabled', () => {
    const select = colorModeSelect.$$('select');
    assertTrue(!!select);

    let colorModeArr = [ColorMode.BLACK_AND_WHITE];
    colorModeSelect.colorModes = colorModeArr;
    flush();

    // Verify the dropdown is disabled when there's only one option.
    assertEquals(1, select.length);
    assertTrue(select.disabled);

    colorModeArr = colorModeArr.concat([ColorMode.GRAYSCALE]);
    colorModeSelect.colorModes = colorModeArr;
    flush();

    // Verify the dropdown is enabled when there's more than one option.
    assertEquals(2, select.length);
    assertFalse(select.disabled);
  });

  test('colorModesSortedAlphabetically', () => {
    colorModeSelect.colorModes =
        [ColorMode.GRAYSCALE, ColorMode.BLACK_AND_WHITE, ColorMode.COLOR];
    flush();

    // Verify the color modes are sorted alphabetically and that black and white
    // is selected by default.
    assertOrderedAlphabetically(
        colorModeSelect.colorModes,
        (colorMode) => getColorModeString(colorMode));
    assertEquals(
        ColorMode.BLACK_AND_WHITE.toString(),
        colorModeSelect.selectedColorMode);
  });

  test('firstColorModeUsedWhenDefaultNotAvailable', () => {
    colorModeSelect.colorModes = [ColorMode.GRAYSCALE, ColorMode.COLOR];
    flush();

    // Verify the first color mode in the sorted color mode array is selected by
    // default when black and white is not an available option.
    assertEquals(ColorMode.COLOR.toString(), colorModeSelect.selectedColorMode);
  });
}
