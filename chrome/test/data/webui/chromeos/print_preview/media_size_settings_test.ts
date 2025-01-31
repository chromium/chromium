// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {MediaSizeCapability, PrintPreviewMediaSizeSettingsElement} from 'chrome://print/print_preview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {getCddTemplate} from './print_preview_test_utils.js';

suite('MediaSizeSettingsTest', function() {
  let mediaSizeSection: PrintPreviewMediaSizeSettingsElement;

  const mediaSizeCapability: MediaSizeCapability =
      getCddTemplate('FooPrinter').capabilities!.printer!.media_size!;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({isBorderlessPrintingEnabled: true});
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    mediaSizeSection =
        document.createElement('print-preview-media-size-settings');
    mediaSizeSection.settings = model.settings;
    mediaSizeSection.capability = mediaSizeCapability;
    mediaSizeSection.disabled = false;
    model.set('settings.mediaSize.available', true);
    model.set('settings.borderless.available', true);
    fakeDataBind(model, mediaSizeSection, 'settings');
    document.body.appendChild(mediaSizeSection);
  });

  test('settings select', function() {
    const settingsSelect = mediaSizeSection.shadowRoot!.querySelector(
        'print-preview-settings-select')!;
    assertFalse(settingsSelect.disabled);
    assertEquals(mediaSizeCapability, settingsSelect.capability);
    assertEquals('mediaSize', settingsSelect.settingName);
  });

  test('update from setting', function() {
    const letterOption = mediaSizeCapability.option[0]!;
    const squareOption = mediaSizeCapability.option[1]!;
    const legalOption = mediaSizeCapability.option[2]!;
    const fourbysixOption = mediaSizeCapability.option[3]!;

    // Default is letter
    const settingsSelect = mediaSizeSection.shadowRoot!.querySelector(
        'print-preview-settings-select')!;
    const borderlessCheckbox =
        mediaSizeSection.shadowRoot!.querySelector('cr-checkbox')!;
    assertDeepEquals(letterOption, JSON.parse(settingsSelect.selectedValue));
    assertDeepEquals(
        letterOption, mediaSizeSection.getSettingValue('mediaSize'));
    assertTrue(borderlessCheckbox.disabled);
    assertFalse(borderlessCheckbox.checked);

    // Change to square
    mediaSizeSection.setSetting('mediaSize', mediaSizeCapability.option[1]!);
    assertDeepEquals(squareOption, JSON.parse(settingsSelect.selectedValue));
    assertFalse(borderlessCheckbox.disabled);
    assertFalse(borderlessCheckbox.checked);

    // Enable the option for borderless printing.
    mediaSizeSection.setSetting('borderless', true);
    assertFalse(borderlessCheckbox.disabled);
    assertTrue(borderlessCheckbox.checked);

    // Change to legal
    mediaSizeSection.setSetting('mediaSize', mediaSizeCapability.option[2]!);
    assertDeepEquals(legalOption, JSON.parse(settingsSelect.selectedValue));
    assertTrue(borderlessCheckbox.disabled);
    assertFalse(borderlessCheckbox.checked);

    // Change to 4x6
    mediaSizeSection.setSetting('mediaSize', mediaSizeCapability.option[3]!);
    assertDeepEquals(fourbysixOption, JSON.parse(settingsSelect.selectedValue));
    assertTrue(borderlessCheckbox.disabled);
    assertTrue(borderlessCheckbox.checked);

    // Set the setting to an option that is not supported by the
    // printer. This can occur if sticky settings are for a different
    // printer at startup.
    const unavailableOption = {
      name: 'ISO_A4',
      width_microns: 210000,
      height_microns: 297000,
      custom_display_name: 'A4',
    };
    mediaSizeSection.setSetting('mediaSize', unavailableOption);

    // The section should reset the setting to the printer's default
    // value, since the printer does not support A4.
    assertDeepEquals(
        letterOption, mediaSizeSection.getSettingValue('mediaSize'));
    assertDeepEquals(letterOption, JSON.parse(settingsSelect.selectedValue));
  });
});
