// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/page_size_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PageSize} from 'chrome://scanning/scanning.mojom-webui.js';
import {getPageSizeString} from 'chrome://scanning/scanning_app_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {assertOrderedAlphabetically, changeSelect} from './scanning_app_test_utils.js';


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
        /** @type {!HTMLSelectElement} */ (
            pageSizeSelect.shadowRoot.querySelector('select'));
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstPageSize = PageSize.kIsoA4;
    const secondPageSize = PageSize.kMax;
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
        (pageSize) => getPageSizeString(pageSize));
    assertEquals(
        PageSize.kMax,
        pageSizeSelect.options[pageSizeSelect.options.length - 1]);
    assertEquals(PageSize.kNaLetter.toString(), pageSizeSelect.selectedOption);
  });

  test('firstPageSizeUsedWhenDefaultNotAvailable', () => {
    pageSizeSelect.options = [PageSize.kMax, PageSize.kIsoA4];
    flush();

    // Verify the first page size in the sorted page sizes array is selected by
    // default when Letter is not an available option.
    assertEquals(PageSize.kIsoA4.toString(), pageSizeSelect.selectedOption);
  });

  // Verify the correct default option is selected when a scanner is selected
  // and the options change.
  test('selectDefaultWhenOptionsChange', () => {
    const select =
        /** @type {!HTMLSelectElement} */ (
            pageSizeSelect.shadowRoot.querySelector('select'));
    pageSizeSelect.options =
        [PageSize.kNaLetter, PageSize.kMax, PageSize.kIsoA4];
    flush();
    return changeSelect(select, /* value */ null, /* selectedIndex */ 0)
        .then(() => {
          assertEquals(
              PageSize.kIsoA4.toString(), pageSizeSelect.selectedOption);
          assertEquals(
              PageSize.kIsoA4.toString(),
              select.options[select.selectedIndex].value);

          pageSizeSelect.options = [PageSize.kNaLetter, PageSize.kMax];
          flush();
          assertEquals(
              PageSize.kNaLetter.toString(), pageSizeSelect.selectedOption);
          assertEquals(
              PageSize.kNaLetter.toString(),
              select.options[select.selectedIndex].value);
        });
  });
});
