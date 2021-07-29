// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import {assert} from 'chrome://resources/js/assert.m.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise, fakeDataBind} from '../test_util.m.js';

import {selectOption} from './print_preview_test_utils.js';

suite('LayoutSettingsTest', function() {
  /** @type {!PrintPreviewLayoutSettingsElement} */
  let layoutSection;

  /** @override */
  setup(function() {
    document.body.innerHTML = '';
    const model = /** @type {!PrintPreviewModelElement} */ (
        document.createElement('print-preview-model'));
    document.body.appendChild(model);

    layoutSection = /** @type {!PrintPreviewLayoutSettingsElement} */ (
        document.createElement('print-preview-layout-settings'));
    layoutSection.settings = model.settings;
    layoutSection.disabled = false;
    fakeDataBind(model, layoutSection, 'settings');
    document.body.appendChild(layoutSection);
  });

  // Tests that setting the setting updates the UI.
  test('set setting', async () => {
    const select = layoutSection.$$('select');
    assertEquals('portrait', select.value);

    layoutSection.setSetting('layout', true);
    await eventToPromise('process-select-change', layoutSection);
    assertEquals('landscape', select.value);
  });

  // Tests that selecting a new option in the dropdown updates the setting.
  test('select option', async () => {
    // Verify that the selected option and names are as expected.
    const select = layoutSection.$$('select');
    assertEquals('portrait', select.value);
    assertFalse(
        /** @type {boolean} */ (layoutSection.getSettingValue('layout')));
    assertFalse(layoutSection.getSetting('layout').setFromUi);
    assertEquals(2, select.options.length);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(layoutSection, 'landscape');
    assertTrue(
        /** @type {boolean} */ (layoutSection.getSettingValue('layout')));
    assertTrue(layoutSection.getSetting('layout').setFromUi);
  });
});
