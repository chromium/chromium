// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/color_mode_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getColorModeString} from 'chrome://scanning/scanning_app_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {assertOrderedAlphabetically, changeSelect} from './scanning_app_test_utils.js';

const ColorMode = {
  BLACK_AND_WHITE: ash.scanning.mojom.ColorMode.kBlackAndWhite,
  GRAYSCALE: ash.scanning.mojom.ColorMode.kGrayscale,
  COLOR: ash.scanning.mojom.ColorMode.kColor,
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
    // Before options are added, the dropdown should be enabled and empty.
    const select = colorModeSelect.$$('select');
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstColorMode = ColorMode.COLOR;
    const secondColorMode = ColorMode.GRAYSCALE;
    colorModeSelect.options = [firstColorMode, secondColorMode];
    flush();

    // Verify that adding color modes results in the dropdown displaying the
    // correct options.
    assertEquals(2, select.length);
    assertEquals(
        getColorModeString(firstColorMode),
        select.options[0].textContent.trim());
    assertEquals(
        getColorModeString(secondColorMode),
        select.options[1].textContent.trim());
    assertEquals(firstColorMode.toString(), select.value);
  });

  test('colorModesSortedAlphabetically', () => {
    colorModeSelect.options =
        [ColorMode.GRAYSCALE, ColorMode.BLACK_AND_WHITE, ColorMode.COLOR];
    flush();

    // Verify the color modes are sorted alphabetically and that color is
    // selected by default.
    assertOrderedAlphabetically(
        colorModeSelect.options, (colorMode) => getColorModeString(colorMode));
    assertEquals(ColorMode.COLOR.toString(), colorModeSelect.selectedOption);
  });

  test('firstColorModeUsedWhenDefaultNotAvailable', () => {
    colorModeSelect.options = [ColorMode.GRAYSCALE, ColorMode.BLACK_AND_WHITE];
    flush();

    // Verify the first color mode in the sorted color mode array is selected by
    // default when color is not an available option.
    assertEquals(
        ColorMode.BLACK_AND_WHITE.toString(), colorModeSelect.selectedOption);
  });

  // Verify the correct default option is selected when a scanner is selected
  // and the options change.
  test('selectDefaultWhenOptionsChange', () => {
    const select =
        /** @type {!HTMLSelectElement} */ (colorModeSelect.$$('select'));
    colorModeSelect.options =
        [ColorMode.GRAYSCALE, ColorMode.BLACK_AND_WHITE, ColorMode.COLOR];
    flush();
    return changeSelect(select, /* value */ null, /* selectedIndex */ 0)
        .then(() => {
          assertEquals(
              ColorMode.BLACK_AND_WHITE.toString(),
              colorModeSelect.selectedOption);
          assertEquals(
              ColorMode.BLACK_AND_WHITE.toString(),
              select.options[select.selectedIndex].value);

          colorModeSelect.options = [ColorMode.GRAYSCALE, ColorMode.COLOR];
          flush();
          assertEquals(
              ColorMode.COLOR.toString(), colorModeSelect.selectedOption);
          assertEquals(
              ColorMode.COLOR.toString(),
              select.options[select.selectedIndex].value);
        });
  });
}
