// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/source_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getSourceTypeString} from 'chrome://scanning/scanning_app_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {assertOrderedAlphabetically, createScannerSource} from './scanning_app_test_utils.js';

const FileType = {
  JPG: chromeos.scanning.mojom.FileType.kJpg,
  PDF: chromeos.scanning.mojom.FileType.kPdf,
  PNG: chromeos.scanning.mojom.FileType.kPng,
};

const PageSize = {
  A4: chromeos.scanning.mojom.PageSize.kIsoA4,
  Letter: chromeos.scanning.mojom.PageSize.kNaLetter,
  Max: chromeos.scanning.mojom.PageSize.kMax,
};

const SourceType = {
  FLATBED: chromeos.scanning.mojom.SourceType.kFlatbed,
  ADF_SIMPLEX: chromeos.scanning.mojom.SourceType.kAdfSimplex,
  ADF_DUPLEX: chromeos.scanning.mojom.SourceType.kAdfDuplex,
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
    // Before options are added, the dropdown should be enabled and empty.
    const select = sourceSelect.$$('select');
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(0, select.length);

    const firstSource =
        createScannerSource(SourceType.ADF_SIMPLEX, 'adf simplex', pageSizes);
    const secondSource =
        createScannerSource(SourceType.FLATBED, 'platen', pageSizes);
    const sourceArr = [firstSource, secondSource];
    sourceSelect.sources = sourceArr;
    flush();

    // Verify that adding sources results in the dropdown displaying the correct
    // options.
    assertEquals(2, select.length);
    assertEquals(
        getSourceTypeString(firstSource.type),
        select.options[0].textContent.trim());
    assertEquals(
        getSourceTypeString(secondSource.type),
        select.options[1].textContent.trim());
    assertEquals(secondSource.name, select.value);
  });

  test('sourcesSortedAlphabetically', () => {
    const sources = [
      createScannerSource(SourceType.FLATBED, 'C', pageSizes),
      createScannerSource(SourceType.ADF_DUPLEX, 'B', pageSizes),
      createScannerSource(SourceType.FLATBED, 'D', pageSizes),
      createScannerSource(SourceType.ADF_DUPLEX, 'A', pageSizes),
    ];
    sourceSelect.sources = sources;
    flush();
    assertOrderedAlphabetically(
        sourceSelect.sources, (source) => getSourceTypeString(source.type));
  });

  test('flatbedSelectedByDefaultIfProvided', () => {
    const sources = [
      createScannerSource(SourceType.FLATBED, 'C', pageSizes),
      createScannerSource(SourceType.ADF_SIMPLEX, 'B', pageSizes),
      createScannerSource(SourceType.ADF_DUPLEX, 'A', pageSizes),
    ];
    sourceSelect.sources = sources;
    flush();
    const flatbedSource =
        sourceSelect.sources.find(source => source.type === SourceType.FLATBED);
    assertEquals(sourceSelect.selectedSource, flatbedSource.name);
  });

  test('firstSourceUsedWhenFlatbedNotProvided', () => {
    const sources = [
      createScannerSource(SourceType.ADF_SIMPLEX, 'C', pageSizes),
      createScannerSource(SourceType.ADF_DUPLEX, 'B', pageSizes),
    ];
    sourceSelect.sources = sources;
    flush();
    assertEquals(sourceSelect.selectedSource, sourceSelect.sources[0].name);
  });
}
