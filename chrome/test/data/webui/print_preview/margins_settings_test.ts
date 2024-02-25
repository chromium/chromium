// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewMarginsSettingsElement, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {MarginsType, State} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {selectOption} from './print_preview_test_utils.js';

suite('MarginsSettingsTest', function() {
  let marginsSection: PrintPreviewMarginsSettingsElement;

  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    marginsSection = document.createElement('print-preview-margins-settings');
    document.body.appendChild(marginsSection);
    marginsSection.settings = model.settings;
    marginsSection.disabled = false;
    marginsSection.state = State.READY;
    fakeDataBind(model, marginsSection, 'settings');
  });

  // Tests that setting the setting updates the UI.
  test('set setting', async () => {
    const select = marginsSection.shadowRoot!.querySelector('select')!;
    assertEquals(MarginsType.DEFAULT.toString(), select.value);

    marginsSection.setSetting('margins', MarginsType.MINIMUM);
    await eventToPromise('process-select-change', marginsSection);
    assertEquals(MarginsType.MINIMUM.toString(), select.value);
  });

  // Tests that selecting a new option in the dropdown updates the setting.
  test('select option', async () => {
    // Verify that the selected option and names are as expected.
    const select = marginsSection.shadowRoot!.querySelector('select')!;
    assertEquals(MarginsType.DEFAULT.toString(), select.value);
    assertEquals(
        MarginsType.DEFAULT, marginsSection.getSettingValue('margins'));
    assertEquals(4, select.options.length);
    assertFalse(marginsSection.getSetting('margins').setFromUi);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(marginsSection, MarginsType.MINIMUM.toString());
    assertEquals(
        MarginsType.MINIMUM, marginsSection.getSettingValue('margins'));
    assertTrue(marginsSection.getSetting('margins').setFromUi);
  });

  // This test verifies that changing pages per sheet to N > 1 disables the
  // margins dropdown and changes the value to DEFAULT.
  test('disabled by pages per sheet', async () => {
    const select = marginsSection.shadowRoot!.querySelector('select')!;
    await selectOption(marginsSection, MarginsType.MINIMUM.toString());
    assertEquals(
        MarginsType.MINIMUM, marginsSection.getSettingValue('margins'));
    assertFalse(select.disabled);

    model.set('settings.pagesPerSheet.value', 2);
    await eventToPromise('process-select-change', marginsSection);
    assertEquals(
        MarginsType.DEFAULT, marginsSection.getSettingValue('margins'));
    assertEquals(MarginsType.DEFAULT.toString(), select.value);
    assertTrue(select.disabled);

    model.set('settings.pagesPerSheet.value', 1);
    assertEquals(
        MarginsType.DEFAULT, marginsSection.getSettingValue('margins'));
    assertFalse(select.disabled);
  });

  // Test that changing the layout or media size setting clears a custom
  // margins setting.
  test('custom margins cleared by layout and media size', async () => {
    const select = marginsSection.shadowRoot!.querySelector('select')!;
    await selectOption(marginsSection, MarginsType.CUSTOM.toString());
    assertEquals(MarginsType.CUSTOM, marginsSection.getSettingValue('margins'));

    // Changing layout clears custom margins.
    model.set('settings.layout.value', true);
    await eventToPromise('process-select-change', marginsSection);
    assertEquals(
        MarginsType.DEFAULT, marginsSection.getSettingValue('margins'));
    assertEquals(MarginsType.DEFAULT.toString(), select.value);

    await selectOption(marginsSection, MarginsType.CUSTOM.toString());
    assertEquals(MarginsType.CUSTOM, marginsSection.getSettingValue('margins'));

    // Changing media size clears custom margins.
    model.set(
        'settings.mediaSize.value',
        '{height_microns: 400, width_microns: 300}');
    await eventToPromise('process-select-change', marginsSection);
    assertEquals(
        MarginsType.DEFAULT, marginsSection.getSettingValue('margins'));
    assertEquals(MarginsType.DEFAULT.toString(), select.value);
  });
});
