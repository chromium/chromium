// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/page_size_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getPageSizeString} from 'chrome://scanning/scanning_app_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {assertOrderedAlphabetically, changeSelect} from './scanning_app_test_utils.js';

const PageSize = {
  A3: ash.scanning.mojom.PageSize.kIsoA3,
  A4: ash.scanning.mojom.PageSize.kIsoA4,
  B4: ash.scanning.mojom.PageSize.kIsoB4,
  Legal: ash.scanning.mojom.PageSize.kLegal,
  Letter: ash.scanning.mojom.PageSize.kNaLetter,
  Tabloid: ash.scanning.mojom.PageSize.kTabloid,
  Max: ash.scanning.mojom.PageSize.kMax,
};

suite('pageSizeSelectTest', function() {
  /** @type {?PageSizeSelectElement} */
  let pageSizeSelect = null;

  setup(() => {
    pageSizeSelect = /** @type {!PageSizeSelectElement} */ (
        document.createElement('page-size-select'));
    assertTrue(!!pageSizeSelect);
    document.body.appendChild(pageSizeSelect);
  });

  teardown(() => {
    pageSizeSelect.remove();
    pageSizeSelect = null;
  });

  // Verify that adding page sizes results in the dropdown displaying the
  // correct options.
  test('initializePageSizeSelect', () => {
    // Before options are added, the dropdown should be enabled and empty.
    const select =
        /** @type {!HTMLSelectElement} */ (pageSizeSelect.$$('select'));
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstPageSize = PageSize.A4;
    const secondPageSize = PageSize.Max;
    pageSizeSelect.options = [firstPageSize, secondPageSize];
    flush();

    assertEquals(2, select.length);
    assertEquals(
        getPageSizeString(firstPageSize), select.options[0].textContent.trim());
    assertEquals(
        getPageSizeString(secondPageSize),
        select.options[1].textContent.trim());
    assertEquals(firstPageSize.toString(), select.value);

    // Selecting a different option should update the selected value.
    return changeSelect(
               select, secondPageSize.toString(), /* selectedIndex */ null)
        .then(() => {
          assertEquals(
              secondPageSize.toString(), pageSizeSelect.selectedOption);
        });
  });

  // Verify the pages sizes are sorted correctly.
  test('pageSizesSortedCorrectly', () => {
    pageSizeSelect.options = [
      PageSize.Tabloid,
      PageSize.Letter,
      PageSize.A3,
      PageSize.Max,
      PageSize.Legal,
      PageSize.B4,
      PageSize.A4,
    ];
    flush();

    // Verify the page sizes are sorted alphabetically except for the fit to
    // scan area option, which should always be last. Verify that Letter is
    // selected by default.
    assertOrderedAlphabetically(
        pageSizeSelect.options.slice(0, pageSizeSelect.options.length - 1),
        (pageSize) => getPageSizeString(pageSize));
    assertEquals(
        PageSize.Max,
        pageSizeSelect.options[pageSizeSelect.options.length - 1]);
    assertEquals(PageSize.Letter.toString(), pageSizeSelect.selectedOption);
  });

  test('firstPageSizeUsedWhenDefaultNotAvailable', () => {
    pageSizeSelect.options = [PageSize.Max, PageSize.A4];
    flush();

    // Verify the first page size in the sorted page sizes array is selected by
    // default when Letter is not an available option.
    assertEquals(PageSize.A4.toString(), pageSizeSelect.selectedOption);
  });

  // Verify the correct default option is selected when a scanner is selected
  // and the options change.
  test('selectDefaultWhenOptionsChange', () => {
    const select =
        /** @type {!HTMLSelectElement} */ (pageSizeSelect.$$('select'));
    pageSizeSelect.options = [PageSize.Letter, PageSize.Max, PageSize.A4];
    flush();
    return changeSelect(select, /* value */ null, /* selectedIndex */ 0)
        .then(() => {
          assertEquals(PageSize.A4.toString(), pageSizeSelect.selectedOption);
          assertEquals(
              PageSize.A4.toString(),
              select.options[select.selectedIndex].value);

          pageSizeSelect.options = [PageSize.Letter, PageSize.Max];
          flush();
          assertEquals(
              PageSize.Letter.toString(), pageSizeSelect.selectedOption);
          assertEquals(
              PageSize.Letter.toString(),
              select.options[select.selectedIndex].value);
        });
  });
});
