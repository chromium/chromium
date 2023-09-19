// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/color_mode_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ColorMode} from 'chrome://scanning/scanning.mojom-webui.js';
import {getColorModeString} from 'chrome://scanning/scanning_app_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {assertOrderedAlphabetically, changeSelect} from './scanning_app_test_utils.js';

suite('colorModeSelectTest', function() {
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

  // Verify that adding color modes results in the dropdown displaying the
  // correct options.
  test('initializeColorModeSelect', () => {
    // Before options are added, the dropdown should be enabled and empty.
    const select = colorModeSelect.shadowRoot.querySelector('select');
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstColorMode = ColorMode.kColor;
    const secondColorMode = ColorMode.kGrayscale;
    colorModeSelect.options = [firstColorMode, secondColorMode];
    flush();

    assertEquals(2, select.length);
    assertEquals(
        getColorModeString(firstColorMode),
        select.options[0].textContent.trim());
    assertEquals(
        getColorModeString(secondColorMode),
        select.options[1].textContent.trim());
    assertEquals(firstColorMode.toString(), select.value);
  });

  // Verify the color modes are sorted alphabetically and that Color is
  // selected by default.
  test('colorModesSortedAlphabetically', () => {
    colorModeSelect.options =
        [ColorMode.kGrayscale, ColorMode.kBlackAndWhite, ColorMode.kColor];
    flush();

    assertOrderedAlphabetically(
        colorModeSelect.options, (colorMode) => getColorModeString(colorMode));
    assertEquals(ColorMode.kColor.toString(), colorModeSelect.selectedOption);
  });

  // Verify the first color mode in the sorted color mode array is selected by
  // default when Color is not an available option.
  test('firstColorModeUsedWhenDefaultNotAvailable', () => {
    colorModeSelect.options = [ColorMode.kGrayscale, ColorMode.kBlackAndWhite];
    flush();

    assertEquals(
        ColorMode.kBlackAndWhite.toString(), colorModeSelect.selectedOption);
  });

  // Verify the correct default option is selected when a scanner is selected
  // and the options change.
  test('selectDefaultWhenOptionsChange', () => {
    const select =
        /** @type {!HTMLSelectElement} */ (
            colorModeSelect.shadowRoot.querySelector('select'));
    colorModeSelect.options =
        [ColorMode.kGrayscale, ColorMode.kBlackAndWhite, ColorMode.kColor];
    flush();
    return changeSelect(select, /* value */ null, /* selectedIndex */ 0)
        .then(() => {
          assertEquals(
              ColorMode.kBlackAndWhite.toString(),
              colorModeSelect.selectedOption);
          assertEquals(
              ColorMode.kBlackAndWhite.toString(),
              select.options[select.selectedIndex].value);

          colorModeSelect.options = [ColorMode.kGrayscale, ColorMode.kColor];
          flush();
          assertEquals(
              ColorMode.kColor.toString(), colorModeSelect.selectedOption);
          assertEquals(
              ColorMode.kColor.toString(),
              select.options[select.selectedIndex].value);
        });
  });
});
