// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/resolution_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {changeSelect} from './scanning_app_test_utils.js';

export function resolutionSelectTest() {
  /** @type {?ResolutionSelectElement} */
  let resolutionSelect = null;

  setup(() => {
    resolutionSelect = /** @type {!ResolutionSelectElement} */ (
        document.createElement('resolution-select'));
    assertTrue(!!resolutionSelect);
    document.body.appendChild(resolutionSelect);
  });

  teardown(() => {
    resolutionSelect.remove();
    resolutionSelect = null;
  });

  test('initializeResolutionSelect', () => {
    // Before options are added, the dropdown should be enabled and empty.
    const select =
        /** @type {!HTMLSelectElement} */ (resolutionSelect.$$('select'));
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstResolution = 75;
    const secondResolution = 300;
    resolutionSelect.resolutions = [firstResolution, secondResolution];
    flush();

    // Verify that adding resolutions results in the dropdown displaying the
    // correct options.
    assertEquals(2, select.length);
    assertEquals(
        secondResolution.toString() + ' dpi',
        select.options[0].textContent.trim());
    assertEquals(
        firstResolution.toString() + ' dpi',
        select.options[1].textContent.trim());
    assertEquals(secondResolution.toString(), select.value);

    // Selecting a different option should update the selected value.
    return changeSelect(
               select, secondResolution.toString(), /* selectedIndex */ null)
        .then(() => {
          assertEquals(
              secondResolution.toString(), resolutionSelect.selectedResolution);
        });
  });

  test('resolutionsSortedCorrectly', () => {
    resolutionSelect.resolutions = [150, 300, 75, 600, 1200, 200];
    flush();

    // Verify the resolutions are sorted in descending order and that 300 is
    // selected by default.
    for (let i = 0; i < resolutionSelect.resolutions.length - 1; i++) {
      assert(
          resolutionSelect.resolutions[i] >
          resolutionSelect.resolutions[i + 1]);
    }
    assertEquals('300', resolutionSelect.selectedResolution);
  });

  test('firstResolutionUsedWhenDefaultNotAvailable', () => {
    resolutionSelect.resolutions = [150, 75, 600, 1200, 200];
    flush();

    // Verify the first resolution in the sorted resolution array is selected by
    // default when 300 is not an available option.
    assertEquals('1200', resolutionSelect.selectedResolution);
  });
}
