// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {MediaTypeCapability, PrintPreviewMediaTypeSettingsElement} from 'chrome://print/print_preview.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {getCddTemplate} from './print_preview_test_utils.js';

suite('MediaTypeSettingsTest', function() {
  let mediaTypeSection: PrintPreviewMediaTypeSettingsElement;

  const mediaTypeCapability: MediaTypeCapability =
      getCddTemplate('FooPrinter').capabilities!.printer!.media_type!;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({isBorderlessPrintingEnabled: true});
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    mediaTypeSection =
        document.createElement('print-preview-media-type-settings');
    mediaTypeSection.settings = model.settings;
    mediaTypeSection.capability = mediaTypeCapability;
    mediaTypeSection.disabled = false;
    model.set('settings.mediaType.available', true);
    fakeDataBind(model, mediaTypeSection, 'settings');
    document.body.appendChild(mediaTypeSection);
  });

  test('settings select', function() {
    const settingsSelect = mediaTypeSection.shadowRoot!.querySelector(
        'print-preview-settings-select')!;
    assertFalse(settingsSelect.disabled);
    assertEquals(mediaTypeCapability, settingsSelect.capability);
    assertEquals('mediaType', settingsSelect.settingName);
  });

  test('update from setting', function() {
    const plainOption = mediaTypeCapability.option[0]!;
    const photoOption = mediaTypeCapability.option[1]!;

    // Default is plain
    const settingsSelect = mediaTypeSection.shadowRoot!.querySelector(
        'print-preview-settings-select')!;
    assertDeepEquals(plainOption, JSON.parse(settingsSelect.selectedValue));
    assertDeepEquals(
        plainOption, mediaTypeSection.getSettingValue('mediaType'));

    // Change to photo
    mediaTypeSection.setSetting('mediaType', mediaTypeCapability.option[1]!);
    assertDeepEquals(photoOption, JSON.parse(settingsSelect.selectedValue));

    // Set the setting to an option that is not supported by the
    // printer. This can occur if sticky settings are for a different
    // printer at startup.
    const unavailableOption = {
      vendor_id: 'photographic-glossy',
      custom_display_name: 'Glossy Photo',
    };
    mediaTypeSection.setSetting('mediaType', unavailableOption);

    // The section should reset the setting to the printer's default
    // value, since the printer does not support glossy photo paper.
    assertDeepEquals(
        plainOption, mediaTypeSection.getSettingValue('mediaType'));
    assertDeepEquals(plainOption, JSON.parse(settingsSelect.selectedValue));
  });
});
