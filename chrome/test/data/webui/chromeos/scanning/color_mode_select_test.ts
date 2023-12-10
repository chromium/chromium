// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/color_mode_select.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ColorModeSelectElement} from 'chrome://scanning/color_mode_select.js';
import {ColorMode} from 'chrome://scanning/scanning.mojom-webui.js';
import {getColorModeString} from 'chrome://scanning/scanning_app_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {assertOrderedAlphabetically, changeSelectedIndex} from './scanning_app_test_utils.js';

suite('colorModeSelectTest', function() {
  let colorModeSelect: ColorModeSelectElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    colorModeSelect = document.createElement('color-mode-select');
    assertTrue(!!colorModeSelect);
    document.body.appendChild(colorModeSelect);
  });

  teardown(() => {
    colorModeSelect?.remove();
    colorModeSelect = null;
  });

  function getSelect(): HTMLSelectElement {
    assert(colorModeSelect);
    const select =
        strictQuery('select', colorModeSelect.shadowRoot, HTMLSelectElement);
    assert(select);
    return select;
  }

  function getOption(index: number): HTMLOptionElement {
    const options = Array.from(getSelect().querySelectorAll('option'));
    assert(index < options.length);
    return options[index]!;
  }

  // Verify that adding color modes results in the dropdown displaying the
  // correct options.
  test('initializeColorModeSelect', () => {
    assert(colorModeSelect);
    // Before options are added, the dropdown should be enabled and empty.
    const select = getSelect();
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstColorMode = ColorMode.kColor;
    const secondColorMode = ColorMode.kGrayscale;
    colorModeSelect.options = [firstColorMode, secondColorMode];
    flush();

    assertEquals(2, select.length);
    assertEquals(
        getColorModeString(firstColorMode), getOption(0).textContent!.trim());
    assertEquals(
        getColorModeString(secondColorMode), getOption(1).textContent!.trim());
    assertEquals(firstColorMode.toString(), select.value);
  });

  // Verify the color modes are sorted alphabetically and that Color is
  // selected by default.
  test('colorModesSortedAlphabetically', () => {
    assert(colorModeSelect);
    colorModeSelect.options =
        [ColorMode.kGrayscale, ColorMode.kBlackAndWhite, ColorMode.kColor];
    flush();

    assertOrderedAlphabetically(
        colorModeSelect.options,
        (colorMode: ColorMode) => getColorModeString(colorMode));
    assertEquals(ColorMode.kColor.toString(), colorModeSelect.selectedOption);
  });

  // Verify the first color mode in the sorted color mode array is selected by
  // default when Color is not an available option.
  test('firstColorModeUsedWhenDefaultNotAvailable', () => {
    assert(colorModeSelect);
    colorModeSelect.options = [ColorMode.kGrayscale, ColorMode.kBlackAndWhite];
    flush();

    assertEquals(
        ColorMode.kBlackAndWhite.toString(), colorModeSelect.selectedOption);
  });

  // Verify the correct default option is selected when a scanner is selected
  // and the options change.
  test('selectDefaultWhenOptionsChange', async () => {
    assert(colorModeSelect);
    const select = getSelect();
    colorModeSelect.options =
        [ColorMode.kGrayscale, ColorMode.kBlackAndWhite, ColorMode.kColor];
    flush();
    await changeSelectedIndex(select, /*index=*/ 0);
    assertEquals(
        ColorMode.kBlackAndWhite.toString(), colorModeSelect.selectedOption);
    assertEquals(
        ColorMode.kBlackAndWhite.toString(),
        getOption(select.selectedIndex).value);

    colorModeSelect.options = [ColorMode.kGrayscale, ColorMode.kColor];
    flush();
    assertEquals(ColorMode.kColor.toString(), colorModeSelect.selectedOption);
    assertEquals(
        ColorMode.kColor.toString(), getOption(select.selectedIndex).value);
  });
});
