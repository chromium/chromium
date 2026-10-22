// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PageOrientationOption, PrintPreviewLayoutSettingsElement, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {Destination, DestinationOrigin} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {getCddTemplateWithAdvancedSettings, selectOption} from './print_preview_test_utils.js';

suite('LayoutSettingsTest', function() {
  let layoutSection: PrintPreviewLayoutSettingsElement;
  let model: PrintPreviewModelElement;

  /** @override */
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    layoutSection = document.createElement('print-preview-layout-settings');
    layoutSection.settings = model.settings;
    layoutSection.disabled = false;
    fakeDataBind(model, layoutSection, 'settings');
    document.body.appendChild(layoutSection);
  });

  // Tests that setting the setting updates the UI.
  test('set setting', () => {
    const select = layoutSection.shadowRoot!.querySelector('select')!;
    assertEquals('portrait', select.value);

    layoutSection.setSetting('layout', true);
    assertEquals('landscape', select.value);
  });

  // Tests that selecting a new option in the dropdown updates the setting.
  test('select option', async () => {
    // Verify that the selected option and names are as expected.
    const select = layoutSection.shadowRoot!.querySelector('select')!;
    assertEquals('portrait', select.value);
    assertFalse(layoutSection.getSettingValue('layout') as boolean);
    assertFalse(layoutSection.getSetting('layout').setFromUi);
    assertEquals(2, select.options.length);

    // Verify that selecting an new option in the dropdown sets the setting.
    await selectOption(layoutSection, 'landscape');
    assertTrue(layoutSection.getSettingValue('layout') as boolean);
    assertTrue(layoutSection.getSetting('layout').setFromUi);
  });

  test('disabled by destination policy', function() {
    const select = layoutSection.shadowRoot!.querySelector('select')!;
    assertFalse(select.disabled);

    layoutSection.set('settings.layout.setByDestinationPolicy', true);
    assertTrue(select.disabled);
  });

  test('switch destinations', function() {
    const testDestination1 =
        new Destination('FooDevice', DestinationOrigin.LOCAL, 'FooName');
    testDestination1.capabilities =
        getCddTemplateWithAdvancedSettings(1, 'FooDevice').capabilities;
    testDestination1.capabilities!.printer.page_orientation = {
      option: [
        {type: 'PORTRAIT'},
        {type: 'LANDSCAPE', is_default: true},
      ] as PageOrientationOption[],
    };

    const testDestination2 =
        new Destination('BarDevice', DestinationOrigin.LOCAL, 'BarName');
    testDestination2.capabilities =
        getCddTemplateWithAdvancedSettings(2, 'BarDevice').capabilities;
    testDestination2.capabilities!.printer.page_orientation = {
      option: [
        {type: 'PORTRAIT', is_default: true},
      ] as PageOrientationOption[],
    };

    model.destination = testDestination1;
    model.applyStickySettings();
    const select = layoutSection.shadowRoot!.querySelector('select')!;
    assertEquals('landscape', select.value);

    model.destination = testDestination2;
    model.applyStickySettings();
    assertEquals('portrait', select.value);
  });
});
