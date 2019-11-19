// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MarginsType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {selectOption} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise, fakeDataBind} from 'chrome://test/test_util.m.js';

suite('PagesPerSheetSettingsTest', function() {
  let pagesPerSheetSection = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    pagesPerSheetSection =
        document.createElement('print-preview-pages-per-sheet-settings');
    pagesPerSheetSection.settings = model.settings;
    pagesPerSheetSection.disabled = false;
    fakeDataBind(model, pagesPerSheetSection, 'settings');
    document.body.appendChild(pagesPerSheetSection);
  });

  // Tests that setting the setting updates the UI.
  test('set setting', async () => {
    const select = pagesPerSheetSection.$$('select');
    assertEquals('1', select.value);

    pagesPerSheetSection.setSetting('pagesPerSheet', 4);
    await eventToPromise('process-select-change', pagesPerSheetSection);
    assertEquals('4', select.value);
  });

  // Tests that setting the pages per sheet setting resets margins to DEFAULT.
  test('resets margins setting', async () => {
    pagesPerSheetSection.setSetting('margins', MarginsType.NO_MARGINS);
    assertEquals(1, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
    pagesPerSheetSection.setSetting('pagesPerSheet', 4);
    await eventToPromise('process-select-change', pagesPerSheetSection);
    assertEquals(4, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
    assertEquals(
        MarginsType.DEFAULT, pagesPerSheetSection.getSettingValue('margins'));
  });

  // Tests that selecting a new option in the dropdown updates the setting.
  test('select option', async () => {
    // Verify that the selected option and names are as expected.
    const select = pagesPerSheetSection.$$('select');
    assertEquals('1', select.value);
    assertEquals(1, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
    assertFalse(pagesPerSheetSection.getSetting('pagesPerSheet').setFromUi);
    assertEquals(6, select.options.length);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(pagesPerSheetSection, '2');
    assertEquals(2, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
    assertTrue(pagesPerSheetSection.getSetting('pagesPerSheet').setFromUi);
  });
});
