// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {getCddTemplate} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {fakeDataBind} from 'chrome://test/test_util.m.js';

suite('DpiSettingsTest', function() {
  /** @type {?PrintPreviewDpiSettingsElement} */
  let dpiSection = null;

  const dpiCapability = getCddTemplate('FooPrinter').capabilities.printer.dpi;

  const expectedCapabilityWithLabels =
      getCddTemplate('FooPrinter').capabilities.printer.dpi;
  expectedCapabilityWithLabels.option.forEach(option => {
    option.name = option.horizontal_dpi.toString() + ' dpi';
  });

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    dpiSection = document.createElement('print-preview-dpi-settings');
    dpiSection.settings = model.settings;
    dpiSection.capability = dpiCapability;
    dpiSection.disabled = false;
    model.set('settings.dpi.available', true);
    fakeDataBind(model, dpiSection, 'settings');
    document.body.appendChild(dpiSection);
  });

  test('settings select', function() {
    const settingsSelect = dpiSection.$$('print-preview-settings-select');
    assertFalse(settingsSelect.disabled);

    assertDeepEquals(expectedCapabilityWithLabels, settingsSelect.capability);
    assertEquals('dpi', settingsSelect.settingName);
  });

  test('update from setting', function() {
    const highQualityOption = dpiCapability.option[0];
    const lowQualityOption = dpiCapability.option[1];
    const highQualityWithLabel = expectedCapabilityWithLabels.option[0];
    const lowQualityWithLabel = expectedCapabilityWithLabels.option[1];

    // Set the setting to the printer default.
    dpiSection.setSetting('dpi', highQualityOption);

    // Default is 200 dpi.
    const settingsSelect = dpiSection.$$('print-preview-settings-select');
    assertDeepEquals(
        highQualityWithLabel, JSON.parse(settingsSelect.selectedValue));
    assertDeepEquals(highQualityOption, dpiSection.getSettingValue('dpi'));

    // Change to 100
    dpiSection.setSetting('dpi', lowQualityOption);
    assertDeepEquals(
        lowQualityWithLabel, JSON.parse(settingsSelect.selectedValue));

    // Set the setting to an option that is not supported by the
    // printer. This can occur if sticky settings are for a different
    // printer at startup.
    const unavailableOption = {
      horizontal_dpi: 400,
      vertical_dpi: 400,
    };
    dpiSection.setSetting('dpi', unavailableOption);

    // The section should reset the setting to the printer's default
    // value with label, since the printer does not support 400 DPI.
    assertDeepEquals(highQualityWithLabel, dpiSection.getSettingValue('dpi'));
    assertDeepEquals(
        highQualityWithLabel, JSON.parse(settingsSelect.selectedValue));
  });
});
