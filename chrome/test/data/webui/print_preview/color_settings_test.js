// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS, isLacros} from 'chrome://resources/js/cr.m.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, fakeDataBind} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('ColorSettingsTest', function() {
  /** @type {!PrintPreviewColorSettingsElement} */
  let colorSection;

  /** @type {!PrintPreviewModelElement} */
  let model;

  /** @override */
  setup(function() {
    document.body.innerHTML = '';
    model = /** @type {!PrintPreviewModelElement} */ (
        document.createElement('print-preview-model'));
    document.body.appendChild(model);

    colorSection = /** @type {!PrintPreviewColorSettingsElement} */ (
        document.createElement('print-preview-color-settings'));
    colorSection.settings = model.settings;
    colorSection.disabled = false;
    fakeDataBind(model, colorSection, 'settings');
    model.set('settings.color.available', true);
    document.body.appendChild(colorSection);
  });

  // Tests that setting the setting updates the UI.
  test('set setting', async () => {
    const select = colorSection.shadowRoot.querySelector('select');
    assertEquals('color', select.value);

    colorSection.setSetting('color', false);
    await eventToPromise('process-select-change', colorSection);
    assertEquals('bw', select.value);
  });

  // Tests that selecting a new option in the dropdown updates the setting.
  test('select option', async () => {
    // Verify that the selected option and names are as expected.
    const select = colorSection.shadowRoot.querySelector('select');
    assertEquals('color', select.value);
    assertTrue(/** @type {boolean} */ (colorSection.getSettingValue('color')));
    assertFalse(colorSection.getSetting('color').setFromUi);
    assertEquals(2, select.options.length);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(colorSection, 'bw');
    assertFalse(/** @type {boolean} */ (colorSection.getSettingValue('color')));
    assertTrue(colorSection.getSetting('color').setFromUi);
  });

  if (isChromeOS || isLacros) {
    // Tests that if the setting is enforced by enterprise policy it is
    // disabled.
    test('disabled by policy', function() {
      // Verify that the selected option and names are as expected.
      const select = colorSection.shadowRoot.querySelector('select');
      assertFalse(select.disabled);

      model.set('settings.color.setByPolicy', true);
      assertTrue(select.disabled);
    });
  }
});
