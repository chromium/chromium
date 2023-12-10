// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/resolution_select.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ResolutionSelectElement} from 'chrome://scanning/resolution_select.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {changeSelectedIndex, changeSelectedValue} from './scanning_app_test_utils.js';

suite('resolutionSelectTest', function() {
  let resolutionSelect: ResolutionSelectElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resolutionSelect = document.createElement('resolution-select');
    assertTrue(!!resolutionSelect);
    document.body.appendChild(resolutionSelect);
  });

  teardown(() => {
    resolutionSelect?.remove();
    resolutionSelect = null;
  });

  function getSelect(): HTMLSelectElement {
    assert(resolutionSelect);
    const select =
        strictQuery('select', resolutionSelect.shadowRoot, HTMLSelectElement);
    assert(select);
    return select;
  }

  function getOption(index: number): HTMLOptionElement {
    const options = Array.from(getSelect().querySelectorAll('option'));
    assert(index < options.length);
    return options[index]!;
  }

  // Verify that adding resolutions results in the dropdown displaying the
  // correct options.
  test('initializeResolutionSelect', async () => {
    assert(resolutionSelect);
    // Before options are added, the dropdown should be enabled and empty.
    const select = getSelect();
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstResolution = 75;
    const secondResolution = 300;
    resolutionSelect.options = [firstResolution, secondResolution];
    flush();

    assertEquals(2, select.length);
    assertEquals(
        secondResolution.toString() + ' dpi', getOption(0).textContent!.trim());
    assertEquals(
        firstResolution.toString() + ' dpi', getOption(1).textContent!.trim());
    assertEquals(secondResolution.toString(), select.value);

    // Selecting a different option should update the selected value.
    await changeSelectedValue(select, secondResolution.toString());
    assertEquals(secondResolution.toString(), resolutionSelect.selectedOption);
  });

  // Verify the resolutions are sorted correctly.
  test('resolutionsSortedCorrectly', () => {
    assert(resolutionSelect);
    resolutionSelect.options = [150, 300, 75, 600, 1200, 200];
    flush();

    // Verify the resolutions are sorted in descending order and that 300 is
    // selected by default.
    for (let i = 0; i < resolutionSelect.options.length - 1; i++) {
      assertTrue(
          resolutionSelect.options[i]! > resolutionSelect.options[i + 1]!);
    }
    assertEquals('300', resolutionSelect.selectedOption);
  });

  test('firstResolutionUsedWhenDefaultNotAvailable', () => {
    assert(resolutionSelect);
    resolutionSelect.options = [150, 75, 600, 1200, 200];
    flush();

    // Verify the first resolution in the sorted resolution array is selected by
    // default when 300 is not an available option.
    assertEquals('1200', resolutionSelect.selectedOption);
  });

  // Verify the correct default option is selected when a scanner is selected
  // and the options change.
  test('selectDefaultWhenOptionsChange', async () => {
    assert(resolutionSelect);
    const select = getSelect();
    resolutionSelect.options = [600, 300, 150];
    flush();
    await changeSelectedIndex(select, /*index=*/ 0);
    assertEquals('600', resolutionSelect.selectedOption);
    assertEquals('600', getOption(select.selectedIndex).value);

    resolutionSelect.options = [300, 150];
    flush();
    assertEquals('300', resolutionSelect.selectedOption);
    assertEquals('300', getOption(select.selectedIndex).value);
  });
});
