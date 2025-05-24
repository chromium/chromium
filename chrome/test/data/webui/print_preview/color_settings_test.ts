// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewColorSettingsElement, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('ColorSettingsTest', function() {
  let colorSection: PrintPreviewColorSettingsElement;

  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    colorSection = document.createElement('print-preview-color-settings');
    colorSection.disabled = false;
    model.setSettingAvailableForTesting('color', true);
    document.body.appendChild(colorSection);
    return microtasksFinished();
  });

  // Tests that setting the setting updates the UI.
  test('set setting', async () => {
    const select = colorSection.shadowRoot.querySelector('select')!;
    assertEquals('color', select.value);

    colorSection.setSetting('color', false);
    await microtasksFinished();
    assertEquals('bw', select.value);
  });

  // Tests that selecting a new option in the dropdown updates the setting.
  test('select option', async () => {
    // Verify that the selected option and names are as expected.
    const select = colorSection.shadowRoot.querySelector('select')!;
    assertEquals('color', select.value);
    assertTrue(colorSection.getSettingValue('color') as boolean);
    assertFalse(colorSection.getSetting('color').setFromUi);
    assertEquals(2, select.options.length);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(colorSection, 'bw');
    assertFalse(colorSection.getSettingValue('color') as boolean);
    assertTrue(colorSection.getSetting('color').setFromUi);
  });
});
