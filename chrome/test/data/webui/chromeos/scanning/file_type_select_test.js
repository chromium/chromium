// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/file_type_select.js';

import {FileType} from 'chrome://scanning/scanning.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {changeSelect} from './scanning_app_test_utils.js';

suite('fileTypeSelectTest', function() {
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

  // Verify the dropdown is initialized as enabled with three options. The
  // default option should be PDF.
  test('initializeFileTypeSelect', () => {
    const select =
        /** @type {!HTMLSelectElement} */ (
            fileTypeSelect.shadowRoot.querySelector('select'));
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(3, select.length);
    assertEquals('JPG', select.options[0].textContent.trim());
    assertEquals('PNG', select.options[1].textContent.trim());
    assertEquals('PDF', select.options[2].textContent.trim());
    assertEquals(FileType.kPdf.toString(), select.value);

    // Selecting a different option should update the selected value.
    return changeSelect(
               select, FileType.kJpg.toString(), /* selectedIndex */ null)
        .then(() => {
          assertEquals(
              FileType.kJpg.toString(), fileTypeSelect.selectedFileType);
        });
  });
});
