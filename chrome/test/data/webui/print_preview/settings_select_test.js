// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getMediaSizeCapabilityWithCustomNames, selectOption} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise, fakeDataBind} from 'chrome://test/test_util.m.js';

suite('SettingsSelectTest', function() {
  /** @type {?PrintPreviewSettingsSelectElement} */
  let settingsSelect = null;

  /** @type {?PrintPreviewModelElement} */
  let model = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    settingsSelect = document.createElement('print-preview-settings-select');
    settingsSelect.settings = model.settings;
    settingsSelect.disabled = false;
    fakeDataBind(model, settingsSelect, 'settings');
    document.body.appendChild(settingsSelect);
  });

  // Test that destinations are correctly displayed in the lists.
  test('custom media names', function() {
    model.set('settings.mediaSize.available', true);

    // Set a capability with custom paper sizes.
    settingsSelect.settingName = 'mediaSize';
    settingsSelect.capability = getMediaSizeCapabilityWithCustomNames();
    const customLocalizedMediaName = settingsSelect.capability.option[0]
                                         .custom_display_name_localized[0]
                                         .value;
    const customMediaName =
        settingsSelect.capability.option[1].custom_display_name;
    flush();

    const select = settingsSelect.$$('select');
    // Verify that the selected option and names are as expected.
    assertEquals(0, select.selectedIndex);
    assertEquals(2, select.options.length);
    assertEquals(
        customLocalizedMediaName, select.options[0].textContent.trim());
    assertEquals(customMediaName, select.options[1].textContent.trim());
  });

  test('set setting', async () => {
    // Fake setting.
    model.set('settings', {
      fruit: {
        value: {},
        unavailableValue: {},
        valid: true,
        available: true,
        setByPolicy: false,
        setFromUi: false,
        key: 'fruit',
      },
    });
    settingsSelect.settingName = 'fruit';
    settingsSelect.capability = {
      option: [
        {name: 'lime', color: 'green', size: 3},
        {name: 'orange', color: 'orange', size: 5, is_default: true},
      ],
    };
    flush();
    const option0 = JSON.stringify(settingsSelect.capability.option[0]);
    const option1 = JSON.stringify(settingsSelect.capability.option[1]);
    const select = settingsSelect.$$('select');

    // Normally done for initialization by the model and parent section.
    settingsSelect.set(
        'settings.fruit.value', settingsSelect.capability.option[1]);
    settingsSelect.selectValue(option1);

    // Verify that the selected option and names are as expected.
    assertEquals(2, select.options.length);
    assertEquals(1, select.selectedIndex);
    assertFalse(settingsSelect.getSetting('fruit').setFromUi);
    assertEquals('lime', select.options[0].textContent.trim());
    assertEquals('orange', select.options[1].textContent.trim());
    assertEquals(option0, select.options[0].value);
    assertEquals(option1, select.options[1].value);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(settingsSelect, option0);
    assertEquals(
        option0, JSON.stringify(settingsSelect.getSettingValue('fruit')));
    assertTrue(settingsSelect.getSetting('fruit').setFromUi);
    assertEquals(0, select.selectedIndex);

    // Verify that selecting from outside works.
    settingsSelect.selectValue(option1);
    await eventToPromise('process-select-change', settingsSelect);
    assertEquals(
        option1, JSON.stringify(settingsSelect.getSettingValue('fruit')));
    assertEquals(1, select.selectedIndex);
  });
});
