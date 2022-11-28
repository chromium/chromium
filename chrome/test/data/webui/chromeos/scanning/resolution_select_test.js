// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/resolution_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {changeSelect} from './scanning_app_test_utils.js';

suite('resolutionSelectTest', function() {
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

  // Verify that adding resolutions results in the dropdown displaying the
  // correct options.
  test('initializeResolutionSelect', () => {
    // Before options are added, the dropdown should be enabled and empty.
    const select =
        /** @type {!HTMLSelectElement} */ (resolutionSelect.$$('select'));
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstResolution = 75;
    const secondResolution = 300;
    resolutionSelect.options = [firstResolution, secondResolution];
    flush();

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
              secondResolution.toString(), resolutionSelect.selectedOption);
        });
  });

  // Verify the resolutions are sorted correctly.
  test('resolutionsSortedCorrectly', () => {
    resolutionSelect.options = [150, 300, 75, 600, 1200, 200];
    flush();

    // Verify the resolutions are sorted in descending order and that 300 is
    // selected by default.
    for (let i = 0; i < resolutionSelect.options.length - 1; i++) {
      assertTrue(resolutionSelect.options[i] > resolutionSelect.options[i + 1]);
    }
    assertEquals('300', resolutionSelect.selectedOption);
  });

  test('firstResolutionUsedWhenDefaultNotAvailable', () => {
    resolutionSelect.options = [150, 75, 600, 1200, 200];
    flush();

    // Verify the first resolution in the sorted resolution array is selected by
    // default when 300 is not an available option.
    assertEquals('1200', resolutionSelect.selectedOption);
  });

  // Verify the correct default option is selected when a scanner is selected
  // and the options change.
  test('selectDefaultWhenOptionsChange', () => {
    const select =
        /** @type {!HTMLSelectElement} */ (resolutionSelect.$$('select'));
    resolutionSelect.options = [600, 300, 150];
    flush();
    return changeSelect(select, /* value */ null, /* selectedIndex */ 0)
        .then(() => {
          assertEquals('600', resolutionSelect.selectedOption);
          assertEquals('600', select.options[select.selectedIndex].value);

          resolutionSelect.options = [300, 150];
          flush();
          assertEquals('300', resolutionSelect.selectedOption);
          assertEquals('300', select.options[select.selectedIndex].value);
        });
  });
});
