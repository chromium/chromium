// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewLayoutSettingsElement} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('LayoutSettingsTest', function() {
  let layoutSection: PrintPreviewLayoutSettingsElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    layoutSection = document.createElement('print-preview-layout-settings');
    layoutSection.disabled = false;
    document.body.appendChild(layoutSection);
    return microtasksFinished();
  });

  // Tests that setting the setting updates the UI.
  test('set setting', async () => {
    const select = layoutSection.shadowRoot.querySelector('select')!;
    assertEquals('portrait', select.value);

    layoutSection.setSetting('layout', true);
    await microtasksFinished();
    assertEquals('landscape', select.value);
  });

  // Tests that selecting a new option in the dropdown updates the setting.
  test('select option', async () => {
    // Verify that the selected option and names are as expected.
    const select = layoutSection.shadowRoot.querySelector('select')!;
    assertEquals('portrait', select.value);
    assertFalse(layoutSection.getSettingValue('layout') as boolean);
    assertFalse(layoutSection.getSetting('layout').setFromUi);
    assertEquals(2, select.options.length);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(layoutSection, 'landscape');
    assertTrue(layoutSection.getSettingValue('layout') as boolean);
    assertTrue(layoutSection.getSetting('layout').setFromUi);
  });
});
