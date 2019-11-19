// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {triggerInputEvent} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {fakeDataBind} from 'chrome://test/test_util.m.js';

suite('PinSettingsTest', function() {
  /** @type {?PrintPreviewPinSettingsElement} */
  let pinSection = null;

  /** @type {?PrintPreviewModelElement} */
  let model = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.set('settings.pin.available', true);
    model.set('settings.pin.value', false);
    model.set('settings.pinValue.available', true);
    model.set('settings.pinValue.value', '');

    pinSection = document.createElement('print-preview-pin-settings');
    pinSection.settings = model.settings;
    pinSection.state = State.READY;
    pinSection.disabled = false;
    fakeDataBind(model, pinSection, 'settings');
    document.body.appendChild(pinSection);
    flush();
  });

  // Tests that checking the box or entering the pin value updates the
  // setting.
  test('enter valid pin value', async () => {
    const checkbox = pinSection.$$('cr-checkbox');
    const collapse = pinSection.$$('iron-collapse');
    assertFalse(checkbox.checked);
    assertFalse(collapse.opened);
    assertFalse(pinSection.getSettingValue('pin'));
    assertFalse(pinSection.getSetting('pin').setFromUi);
    assertEquals('', pinSection.getSettingValue('pinValue'));

    checkbox.checked = true;
    checkbox.dispatchEvent(new CustomEvent('change'));
    assertTrue(collapse.opened);
    assertTrue(pinSection.getSettingValue('pin'));
    assertTrue(pinSection.getSetting('pin').setFromUi);
    assertEquals('', pinSection.getSettingValue('pinValue'));

    const input = pinSection.$$('cr-input');
    assertEquals('', input.value);
    assertFalse(pinSection.getSetting('pinValue').setFromUi);

    // Verify that entering the pin value in the input sets the setting.
    await triggerInputEvent(input, '0000', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('0000', pinSection.getSettingValue('pinValue'));
    assertTrue(pinSection.getSetting('pinValue').setFromUi);
    assertEquals(true, pinSection.getSetting('pinValue').valid);
  });

  // Tests that entering non-digit pin value updates the validity of the
  // setting.
  test('enter non-digit pin value', async () => {
    const checkbox = pinSection.$$('cr-checkbox');
    checkbox.checked = true;
    checkbox.dispatchEvent(new CustomEvent('change'));
    const input = pinSection.$$('cr-input');

    // Verify that entering the non-digit pin value in the input updates the
    // setting validity and doesn't update its value.
    await triggerInputEvent(input, 'aaaa', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('', pinSection.getSettingValue('pinValue'));
    assertEquals(false, pinSection.getSetting('pinValue').valid);

    // Check that checkbox and input are still enabled so user can correct
    // invalid input.
    assertEquals(false, checkbox.disabled);
    assertEquals(false, input.disabled);
  });


  // Tests that entering too short pin value updates the validity of the
  // setting.
  test('enter too short pin value', async () => {
    const checkbox = pinSection.$$('cr-checkbox');
    checkbox.checked = true;
    checkbox.dispatchEvent(new CustomEvent('change'));
    const input = pinSection.$$('cr-input');

    // Verify that entering too short pin value in the input updates the
    // setting validity and doesn't update its value.
    await triggerInputEvent(input, '000', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('', pinSection.getSettingValue('pinValue'));
    assertEquals(false, pinSection.getSetting('pinValue').valid);

    // Check that checkbox and input are still enabled so user can correct
    // invalid input.
    assertEquals(false, checkbox.disabled);
    assertEquals(false, input.disabled);
  });

  // Tests that entering empty pin value updates the validity of the
  // setting.
  test('enter empty pin value', async () => {
    const checkbox = pinSection.$$('cr-checkbox');
    checkbox.checked = true;
    checkbox.dispatchEvent(new CustomEvent('change'));
    const input = pinSection.$$('cr-input');

    // Verify that initial pin value is empty and the setting is invalid.
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('', pinSection.getSettingValue('pinValue'));
    assertEquals(false, pinSection.getSetting('pinValue').valid);

    // Verify that entering the pin value in the input sets the setting.
    await triggerInputEvent(input, '0000', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('0000', pinSection.getSettingValue('pinValue'));
    assertEquals(true, pinSection.getSetting('pinValue').valid);

    // Verify that entering empty pin value in the input updates the
    // setting validity and its value.
    await triggerInputEvent(input, '', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('', pinSection.getSettingValue('pinValue'));
    assertEquals(false, pinSection.getSetting('pinValue').valid);

    // Check that checkbox and input are still enabled so user can correct
    // invalid input.
    assertEquals(false, checkbox.disabled);
    assertEquals(false, input.disabled);

    // Check that after unchecking the checkbox the pin value is valid again.
    checkbox.checked = false;
    checkbox.dispatchEvent(new CustomEvent('change'));
    assertEquals(true, pinSection.getSetting('pinValue').valid);
  });

  // Tests that if settings are enforced by enterprise policy the
  // appropriate UI is disabled.
  test('disabled by policy', function() {
    const checkbox = pinSection.$$('cr-checkbox');
    assertFalse(checkbox.disabled);

    pinSection.setSetting('pin', true);
    const input = pinSection.$$('cr-input');
    assertFalse(input.disabled);

    model.set('settings.pin.setByPolicy', true);
    assertTrue(checkbox.disabled);
    assertFalse(input.disabled);
  });
});
