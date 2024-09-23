// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewModelElement, PrintPreviewPinSettingsElement} from 'chrome://print/print_preview.js';
import {State} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {triggerInputEvent} from './print_preview_test_utils.js';

suite('PinSettingsTest', function() {
  let pinSection: PrintPreviewPinSettingsElement;

  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

  // Pin settings observes |state| which may change
  // regardless of pin availability, When the pin printing enforced by policy
  // the checkbox will be true and the initial pin is empty which is invalid
  // In this scenario setSettingValid assert will fail because the input is
  // invalid and the pin printing is unavailable. This test make sure there is
  // check for avaiablity before calling setSettingValid
  // Regression test for https://crbug.com/1321118
  test('pin settings unavailable with invalid input', async () => {
    pinSection.state = State.NOT_READY;
    model.set('settings.pin.available', false);
    model.set('settings.pin.value', true);
    model.set('settings.pinValue.available', false);
    pinSection.state = State.READY;
  });

  // Tests that checking the box or entering the pin value updates the
  // setting.
  test('enter valid pin value', async () => {
    const checkbox = pinSection.shadowRoot!.querySelector('cr-checkbox')!;
    const collapse = pinSection.shadowRoot!.querySelector('cr-collapse')!;
    assertFalse(checkbox.checked);
    assertFalse(collapse.opened);
    assertFalse(pinSection.getSettingValue('pin'));
    assertFalse(pinSection.getSetting('pin').setFromUi);
    assertEquals('', pinSection.getSettingValue('pinValue'));

    checkbox.checked = true;
    checkbox.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    assertTrue(collapse.opened);
    assertTrue(pinSection.getSettingValue('pin'));
    assertTrue(pinSection.getSetting('pin').setFromUi);
    assertEquals('', pinSection.getSettingValue('pinValue'));

    const input = pinSection.shadowRoot!.querySelector('cr-input')!;
    await input.updateComplete;
    assertEquals('', input.value);
    assertFalse(pinSection.getSetting('pinValue').setFromUi);

    // Verify that entering the pin value in the input sets the setting.
    await triggerInputEvent(input, '0000', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('0000', pinSection.getSettingValue('pinValue'));
    assertTrue(pinSection.getSetting('pinValue').setFromUi);
    assertEquals(true, pinSection.isPinValid);
  });

  // Tests that entering non-digit pin value updates the validity of the
  // setting.
  test('enter non-digit pin value', async () => {
    const checkbox = pinSection.shadowRoot!.querySelector('cr-checkbox')!;
    checkbox.checked = true;
    checkbox.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    const input = pinSection.shadowRoot!.querySelector('cr-input')!;

    // Verify that entering the non-digit pin value in the input updates the
    // setting validity and doesn't update its value.
    await triggerInputEvent(input, 'aaaa', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('', pinSection.getSettingValue('pinValue'));
    assertEquals(false, pinSection.isPinValid);

    // Check that checkbox and input are still enabled so user can correct
    // invalid input.
    assertEquals(false, checkbox.disabled);
    assertEquals(false, input.disabled);
  });


  // Tests that entering too short pin value updates the validity of the
  // setting.
  test('enter too short pin value', async () => {
    const checkbox = pinSection.shadowRoot!.querySelector('cr-checkbox')!;
    checkbox.checked = true;
    checkbox.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    const input = pinSection.shadowRoot!.querySelector('cr-input')!;

    // Verify that entering too short pin value in the input updates the
    // setting validity and doesn't update its value.
    await triggerInputEvent(input, '000', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('', pinSection.getSettingValue('pinValue'));
    assertEquals(false, pinSection.isPinValid);

    // Check that checkbox and input are still enabled so user can correct
    // invalid input.
    assertEquals(false, checkbox.disabled);
    assertEquals(false, input.disabled);
  });

  // Tests that entering empty pin value updates the validity of the
  // setting.
  test('enter empty pin value', async () => {
    const checkbox = pinSection.shadowRoot!.querySelector('cr-checkbox')!;
    checkbox.checked = true;
    checkbox.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    const input = pinSection.shadowRoot!.querySelector('cr-input')!;

    // Verify that initial pin value is empty and the setting is invalid.
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('', pinSection.getSettingValue('pinValue'));
    assertEquals(false, pinSection.isPinValid);

    // Verify that entering the pin value in the input sets the setting.
    await triggerInputEvent(input, '0000', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('0000', pinSection.getSettingValue('pinValue'));
    assertEquals(true, pinSection.isPinValid);

    // Verify that entering empty pin value in the input updates the
    // setting validity and its value.
    await triggerInputEvent(input, '', pinSection);
    assertTrue(pinSection.getSettingValue('pin'));
    assertEquals('', pinSection.getSettingValue('pinValue'));
    assertEquals(false, pinSection.isPinValid);

    // Check that checkbox and input are still enabled so user can correct
    // invalid input.
    assertEquals(false, checkbox.disabled);
    assertEquals(false, input.disabled);

    // Check that after unchecking the checkbox the pin value is valid again.
    checkbox.checked = false;
    checkbox.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    assertEquals(true, pinSection.isPinValid);
  });

  // Tests that if settings are enforced by enterprise policy the
  // appropriate UI is disabled.
  test('disabled by policy', async () => {
    const checkbox = pinSection.shadowRoot!.querySelector('cr-checkbox')!;
    assertFalse(checkbox.disabled);

    pinSection.setSetting('pin', true);
    const input = pinSection.shadowRoot!.querySelector('cr-input')!;
    assertFalse(input.disabled);

    model.set('settings.pin.setByPolicy', true);
    await input.updateComplete;
    assertTrue(checkbox.disabled);
    assertFalse(input.disabled);
  });
});
