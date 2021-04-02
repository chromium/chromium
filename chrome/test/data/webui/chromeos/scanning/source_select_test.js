// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/source_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getSourceTypeString} from 'chrome://scanning/scanning_app_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {assertOrderedAlphabetically, createScannerSource} from './scanning_app_test_utils.js';

const FileType = {
  JPG: ash.scanning.mojom.FileType.kJpg,
  PDF: ash.scanning.mojom.FileType.kPdf,
  PNG: ash.scanning.mojom.FileType.kPng,
};

const PageSize = {
  A4: ash.scanning.mojom.PageSize.kIsoA4,
  Letter: ash.scanning.mojom.PageSize.kNaLetter,
  Max: ash.scanning.mojom.PageSize.kMax,
};

const SourceType = {
  FLATBED: ash.scanning.mojom.SourceType.kFlatbed,
  ADF_SIMPLEX: ash.scanning.mojom.SourceType.kAdfSimplex,
  ADF_DUPLEX: ash.scanning.mojom.SourceType.kAdfDuplex,
};

const pageSizes = [PageSize.A4, PageSize.Letter, PageSize.Max];

export function sourceSelectTest() {
  /** @type {?SourceSelectElement} */
  let sourceSelect = null;

  setup(() => {
    sourceSelect = /** @type {!SourceSelectElement} */ (
        document.createElement('source-select'));
    assertTrue(!!sourceSelect);
    document.body.appendChild(sourceSelect);
  });

  teardown(() => {
    sourceSelect.remove();
    sourceSelect = null;
  });

  test('initializeSourceSelect', () => {
    // Before options are added, the dropdown should be enabled and display the
    // default option.
    const select = sourceSelect.$$('select');
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(1, select.length);

    const firstSource =
        createScannerSource(SourceType.ADF_SIMPLEX, 'adf simplex', pageSizes);
    const secondSource =
        createScannerSource(SourceType.FLATBED, 'platen', pageSizes);
    const sourceArr = [firstSource, secondSource];
    sourceSelect.options = sourceArr;
    flush();

    // Verify that adding sources results in the dropdown displaying the correct
    // options. The expected options are simplex, flatbed, and the hidden
    // default option.
    assertEquals(3, select.length);
    assertEquals(
        getSourceTypeString(firstSource.type),
        select.options[0].textContent.trim());
    assertEquals(
        getSourceTypeString(secondSource.type),
        select.options[1].textContent.trim());
    assertTrue(select.options[2].hidden);
    assertEquals(secondSource.name, select.value);
  });

  test('sourcesSortedAlphabetically', () => {
    const sources = [
      createScannerSource(SourceType.FLATBED, 'C', pageSizes),
      createScannerSource(SourceType.ADF_DUPLEX, 'B', pageSizes),
      createScannerSource(SourceType.FLATBED, 'D', pageSizes),
      createScannerSource(SourceType.ADF_DUPLEX, 'A', pageSizes),
    ];
    sourceSelect.options = sources;
    flush();
    assertOrderedAlphabetically(
        sourceSelect.options, (source) => getSourceTypeString(source.type));
  });

  test('flatbedSelectedByDefaultIfProvided', () => {
    const sources = [
      createScannerSource(SourceType.FLATBED, 'C', pageSizes),
      createScannerSource(SourceType.ADF_SIMPLEX, 'B', pageSizes),
      createScannerSource(SourceType.ADF_DUPLEX, 'A', pageSizes),
    ];
    sourceSelect.options = sources;
    flush();
    const flatbedSource =
        sourceSelect.options.find(source => source.type === SourceType.FLATBED);
    assertEquals(sourceSelect.selectedOption, flatbedSource.name);
  });

  test('firstSourceUsedWhenFlatbedNotProvided', () => {
    const sources = [
      createScannerSource(SourceType.ADF_SIMPLEX, 'C', pageSizes),
      createScannerSource(SourceType.ADF_DUPLEX, 'B', pageSizes),
    ];
    sourceSelect.options = sources;
    flush();
    assertEquals(sourceSelect.selectedOption, sourceSelect.options[0].name);
  });
}
