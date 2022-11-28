// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/source_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getSourceTypeString} from 'chrome://scanning/scanning_app_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {assertOrderedAlphabetically, createScannerSource} from './scanning_app_test_utils.js';

const ColorMode = {
  BLACK_AND_WHITE: ash.scanning.mojom.ColorMode.kBlackAndWhite,
  GRAYSCALE: ash.scanning.mojom.ColorMode.kGrayscale,
  COLOR: ash.scanning.mojom.ColorMode.kColor,
};

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
const colorModes =
    [ColorMode.BLACK_AND_WHITE, ColorMode.GRAYSCALE, ColorMode.COLOR];
const resolutions = [75, 150, 300];

suite('sourceSelectTest', function() {
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

  // Verify that adding sources results in the dropdown displaying the correct
  // options.
  test('initializeSourceSelect', () => {
    // Before options are added, the dropdown should be enabled and empty.
    const select = sourceSelect.$$('select');
    assertTrue(!!select);
    assertFalse(select.disabled);

    const firstSource = createScannerSource(
        SourceType.ADF_SIMPLEX, 'adf simplex', pageSizes, colorModes,
        resolutions);
    const secondSource = createScannerSource(
        SourceType.FLATBED, 'platen', pageSizes, colorModes, resolutions);
    const sourceArr = [firstSource, secondSource];
    sourceSelect.options = sourceArr;
    flush();

    // The expected options are simplex and flatbed.
    assertEquals(2, select.length);
    assertEquals(
        getSourceTypeString(firstSource.type),
        select.options[0].textContent.trim());
    assertEquals(
        getSourceTypeString(secondSource.type),
        select.options[1].textContent.trim());
    assertEquals(secondSource.name, select.value);
  });

  // Verify the sources are sorted alphabetically.
  test('sourcesSortedAlphabetically', () => {
    const sources = [
      createScannerSource(
          SourceType.FLATBED, 'C', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.ADF_DUPLEX, 'B', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.FLATBED, 'D', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.ADF_DUPLEX, 'A', pageSizes, colorModes, resolutions),
    ];
    sourceSelect.options = sources;
    flush();
    assertOrderedAlphabetically(
        sourceSelect.options, (source) => getSourceTypeString(source.type));
  });

  // Verify the default option is selected when available.
  test('flatbedSelectedByDefaultIfProvided', () => {
    const sources = [
      createScannerSource(
          SourceType.FLATBED, 'C', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.ADF_SIMPLEX, 'B', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.ADF_DUPLEX, 'A', pageSizes, colorModes, resolutions),
    ];
    sourceSelect.options = sources;
    flush();
    const flatbedSource =
        sourceSelect.options.find(source => source.type === SourceType.FLATBED);
    assertEquals(sourceSelect.selectedOption, flatbedSource.name);
  });

  // Verify the first option is selected when the default option is not
  // available.
  test('firstSourceUsedWhenFlatbedNotProvided', () => {
    const sources = [
      createScannerSource(
          SourceType.ADF_SIMPLEX, 'C', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.ADF_DUPLEX, 'B', pageSizes, colorModes, resolutions),
    ];
    sourceSelect.options = sources;
    flush();
    assertEquals(sourceSelect.selectedOption, sourceSelect.options[0].name);
  });
});
