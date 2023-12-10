// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/source_select.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ColorMode, PageSize, ScanSource, SourceType} from 'chrome://scanning/scanning.mojom-webui.js';
import {getSourceTypeString} from 'chrome://scanning/scanning_app_util.js';
import {SourceSelectElement} from 'chrome://scanning/source_select.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {assertOrderedAlphabetically, createScannerSource} from './scanning_app_test_utils.js';

const pageSizes = [PageSize.kIsoA4, PageSize.kNaLetter, PageSize.kMax];
const colorModes =
    [ColorMode.kBlackAndWhite, ColorMode.kGrayscale, ColorMode.kColor];
const resolutions = [75, 150, 300];

suite('sourceSelectTest', function() {
  let sourceSelect: SourceSelectElement|null = null;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    sourceSelect = document.createElement('source-select');
    assertTrue(!!sourceSelect);
    document.body.appendChild(sourceSelect);
  });

  teardown(() => {
    sourceSelect?.remove();
    sourceSelect = null;
  });

  // Verify that adding sources results in the dropdown displaying the correct
  // options.
  test('initializeSourceSelect', () => {
    assert(sourceSelect);
    // Before options are added, the dropdown should be enabled and empty.
    const select =
        strictQuery('select', sourceSelect.shadowRoot, HTMLSelectElement);
    assertTrue(!!select);
    assertFalse(select.disabled);

    const firstSource = createScannerSource(
        SourceType.kAdfSimplex, 'adf simplex', pageSizes, colorModes,
        resolutions);
    const secondSource = createScannerSource(
        SourceType.kFlatbed, 'platen', pageSizes, colorModes, resolutions);
    const sourceArr: ScanSource[] = [firstSource, secondSource];
    sourceSelect.options = sourceArr;
    flush();

    // The expected options are simplex and flatbed.
    assertEquals(2, select.length);
    assertEquals(
        getSourceTypeString(firstSource.type),
        select.options[0]!.textContent!.trim());
    assertEquals(
        getSourceTypeString(secondSource.type),
        select.options[1]!.textContent!.trim());
    assertEquals(secondSource.name, select.value);
  });

  // Verify the sources are sorted alphabetically.
  test('sourcesSortedAlphabetically', () => {
    assert(sourceSelect);
    const sources: ScanSource[] = [
      createScannerSource(
          SourceType.kFlatbed, 'C', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.kAdfDuplex, 'B', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.kFlatbed, 'D', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.kAdfDuplex, 'A', pageSizes, colorModes, resolutions),
    ];
    sourceSelect.options = sources;
    flush();
    assertOrderedAlphabetically(
        sourceSelect.options,
        (source: ScanSource) => getSourceTypeString(source.type));
  });

  // Verify the default option is selected when available.
  test('flatbedSelectedByDefaultIfProvided', () => {
    assert(sourceSelect);
    const sources: ScanSource[] = [
      createScannerSource(
          SourceType.kFlatbed, 'C', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.kAdfSimplex, 'B', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.kAdfDuplex, 'A', pageSizes, colorModes, resolutions),
    ];
    sourceSelect.options = sources;
    flush();
    const flatbedSource = sourceSelect.options.find(
        (source: ScanSource) => source.type === SourceType.kFlatbed);
    assert(flatbedSource);
    assertEquals(sourceSelect.selectedOption, flatbedSource.name);
  });

  // Verify the first option is selected when the default option is not
  // available.
  test('firstSourceUsedWhenFlatbedNotProvided', () => {
    assert(sourceSelect);
    const sources: ScanSource[] = [
      createScannerSource(
          SourceType.kAdfSimplex, 'C', pageSizes, colorModes, resolutions),
      createScannerSource(
          SourceType.kAdfDuplex, 'B', pageSizes, colorModes, resolutions),
    ];
    sourceSelect.options = sources;
    flush();
    assertEquals(sourceSelect.selectedOption, sourceSelect.options[0]!.name);
  });
});
