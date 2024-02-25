// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/page_size_select.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PageSizeSelectElement} from 'chrome://scanning/page_size_select.js';
import {PageSize} from 'chrome://scanning/scanning.mojom-webui.js';
import {getPageSizeString} from 'chrome://scanning/scanning_app_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {assertOrderedAlphabetically, changeSelectedIndex, changeSelectedValue} from './scanning_app_test_utils.js';


suite('pageSizeSelectTest', function() {
  let pageSizeSelect: PageSizeSelectElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    pageSizeSelect = document.createElement('page-size-select');
    assertTrue(!!pageSizeSelect);
    document.body.appendChild(pageSizeSelect);
  });

  teardown(() => {
    pageSizeSelect?.remove();
    pageSizeSelect = null;
  });

  function getSelect(): HTMLSelectElement {
    assert(pageSizeSelect);
    const select =
        strictQuery('select', pageSizeSelect.shadowRoot, HTMLSelectElement);
    assert(select);
    return select;
  }

  function getOption(index: number): HTMLOptionElement {
    const options = Array.from(getSelect().querySelectorAll('option'));
    assert(index < options.length);
    return options[index]!;
  }

  // Verify that adding page sizes results in the dropdown displaying the
  // correct options.
  test('initializePageSizeSelect', async () => {
    assert(pageSizeSelect);
    // Before options are added, the dropdown should be enabled and empty.
    const select = getSelect();
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstPageSize = PageSize.kIsoA4;
    const secondPageSize = PageSize.kMax;
    pageSizeSelect.options = [firstPageSize, secondPageSize];
    flush();

    assertEquals(2, select.length);
    assertEquals(
        getPageSizeString(firstPageSize), getOption(0).textContent!.trim());
    assertEquals(
        getPageSizeString(secondPageSize), getOption(1).textContent!.trim());
    assertEquals(firstPageSize.toString(), select.value);

    // Selecting a different option should update the selected value.
    await changeSelectedValue(select, secondPageSize.toString());
    assertEquals(secondPageSize.toString(), pageSizeSelect.selectedOption);
  });

  // Verify the pages sizes are sorted correctly.
  test('pageSizesSortedCorrectly', () => {
    assert(pageSizeSelect);
    pageSizeSelect.options = [
      PageSize.kTabloid,
      PageSize.kNaLetter,
      PageSize.kIsoA3,
      PageSize.kMax,
      PageSize.kLegal,
      PageSize.kIsoB4,
      PageSize.kIsoA4,
    ];
    flush();

    // Verify the page sizes are sorted alphabetically except for the fit to
    // scan area option, which should always be last. Verify that Letter is
    // selected by default.
    assertOrderedAlphabetically(
        pageSizeSelect.options.slice(0, pageSizeSelect.options.length - 1),
        (pageSize: PageSize) => getPageSizeString(pageSize));
    assertEquals(
        PageSize.kMax,
        pageSizeSelect.options[pageSizeSelect.options.length - 1]);
    assertEquals(PageSize.kNaLetter.toString(), pageSizeSelect.selectedOption);
  });

  test('firstPageSizeUsedWhenDefaultNotAvailable', () => {
    assert(pageSizeSelect);
    pageSizeSelect.options = [PageSize.kMax, PageSize.kIsoA4];
    flush();

    // Verify the first page size in the sorted page sizes array is selected by
    // default when Letter is not an available option.
    assertEquals(PageSize.kIsoA4.toString(), pageSizeSelect.selectedOption);
  });

  // Verify the correct default option is selected when a scanner is selected
  // and the options change.
  test('selectDefaultWhenOptionsChange', async () => {
    assert(pageSizeSelect);
    const select = getSelect();
    pageSizeSelect.options =
        [PageSize.kNaLetter, PageSize.kMax, PageSize.kIsoA4];
    flush();
    await changeSelectedIndex(select, /*index=*/ 0);
    assertEquals(PageSize.kIsoA4.toString(), pageSizeSelect.selectedOption);
    assertEquals(
        PageSize.kIsoA4.toString(), getOption(select.selectedIndex).value);

    pageSizeSelect.options = [PageSize.kNaLetter, PageSize.kMax];
    flush();
    assertEquals(PageSize.kNaLetter.toString(), pageSizeSelect.selectedOption);
    assertEquals(
        PageSize.kNaLetter.toString(), getOption(select.selectedIndex).value);
  });
});
