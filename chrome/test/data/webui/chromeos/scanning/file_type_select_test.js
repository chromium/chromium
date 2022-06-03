// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/file_type_select.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {changeSelect} from './scanning_app_test_utils.js';

const FileType = {
  JPG: ash.scanning.mojom.FileType.kJpg,
  PDF: ash.scanning.mojom.FileType.kPdf,
  PNG: ash.scanning.mojom.FileType.kPng,
  SearchablePDF: ash.scanning.mojom.FileType.kSearchablePdf,
};

export function fileTypeSelectTest() {
  /** @type {?FileTypeSelectElement} */
  let fileTypeSelect = null;

  setup(() => {
    fileTypeSelect = /** @type {!FileTypeSelectElement} */ (
        document.createElement('file-type-select'));
    assertTrue(!!fileTypeSelect);
    document.body.appendChild(fileTypeSelect);
  });

  teardown(() => {
    fileTypeSelect.remove();
    fileTypeSelect = null;
  });

  // Verify the dropdown is initialized as enabled with four options. The
  // default option should be PDF.
  test('initializeFileTypeSelect', () => {
    const select =
        /** @type {!HTMLSelectElement} */ (fileTypeSelect.$$('select'));
    assertTrue(!!select);
    assertFalse(select.disabled);

    const scanAppSearchablePdfEnabled =
        loadTimeData.getBoolean('scanAppSearchablePdfEnabled');
    assertEquals(scanAppSearchablePdfEnabled ? 4 : 3, select.length);
    assertEquals('JPG', select.options[0].textContent.trim());
    assertEquals('PNG', select.options[1].textContent.trim());
    assertEquals('PDF', select.options[2].textContent.trim());
    if (scanAppSearchablePdfEnabled) {
      assertEquals('Searchable PDF', select.options[3].textContent.trim());
    }
    assertEquals(FileType.PDF.toString(), select.value);

    // Selecting a different option should update the selected value.
    return changeSelect(
               select, FileType.JPG.toString(), /* selectedIndex */ null)
        .then(() => {
          assertEquals(
              FileType.JPG.toString(), fileTypeSelect.selectedFileType);
        });
  });
}
