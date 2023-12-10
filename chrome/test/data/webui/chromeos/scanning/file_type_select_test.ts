// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/file_type_select.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FileTypeSelectElement} from 'chrome://scanning/file_type_select.js';
import {FileType} from 'chrome://scanning/scanning.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {changeSelectedValue} from './scanning_app_test_utils.js';

suite('fileTypeSelectTest', function() {
  let fileTypeSelect: FileTypeSelectElement|null = null;

  setup(() => {
    fileTypeSelect = document.createElement('file-type-select');
    assertTrue(!!fileTypeSelect);
    document.body.appendChild(fileTypeSelect);
  });

  teardown(() => {
    fileTypeSelect?.remove();
    fileTypeSelect = null;
  });

  function getSelect(): HTMLSelectElement {
    assert(fileTypeSelect);
    const select =
        strictQuery('select', fileTypeSelect.shadowRoot, HTMLSelectElement);
    assert(select);
    return select;
  }

  function getOption(index: number): HTMLOptionElement {
    const options = Array.from(getSelect().querySelectorAll('option'));
    assert(index < options.length);
    return options[index]!;
  }

  // Verify the dropdown is initialized as enabled with three options. The
  // default option should be PDF.
  test('initializeFileTypeSelect', async () => {
    assert(fileTypeSelect);
    const select = getSelect();
    assertTrue(!!select);
    assertFalse(select.disabled);
    assertEquals(3, select.length);
    assertEquals('JPG', getOption(0).textContent!.trim());
    assertEquals('PNG', getOption(1).textContent!.trim());
    assertEquals('PDF', getOption(2).textContent!.trim());
    assertEquals(FileType.kPdf.toString(), select.value);

    // Selecting a different option should update the selected value.
    await changeSelectedValue(select, FileType.kJpg.toString());

    assertEquals(FileType.kJpg.toString(), fileTypeSelect.selectedFileType);
  });
});
