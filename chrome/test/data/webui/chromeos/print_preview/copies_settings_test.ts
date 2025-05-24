// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewCopiesSettingsElement, PrintPreviewModelElement} from 'chrome://print/print_preview.js';
import {DEFAULT_MAX_COPIES} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {triggerInputEvent} from './print_preview_test_utils.js';

suite('CopiesSettingsTest', function() {
  let copiesSection: PrintPreviewCopiesSettingsElement;

  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);
    model.set('settings.collate.available', true);

    copiesSection = document.createElement('print-preview-copies-settings');
    copiesSection.settings = model.settings;
    copiesSection.disabled = false;
    fakeDataBind(model, copiesSection, 'settings');
    document.body.appendChild(copiesSection);
  });

  /**
   * Confirms that |max| is currently set as copiesSection's maxCopies.
   * @param max Expected maximum copies value to check.
   */
  async function checkCopiesMax(max: number) {
    const input =
        copiesSection.shadowRoot!
            .querySelector('print-preview-number-settings-section')!.getInput();

    // Check that |max| copies is valid.
    await triggerInputEvent(input, max.toString(), copiesSection);
    assertTrue(copiesSection.getSetting('copies').valid);

    // Check that |max| + 1 copies is invalid.
    await triggerInputEvent(input, (max + 1).toString(), copiesSection);
    assertFalse(copiesSection.getSetting('copies').valid);
  }

  // Verifies that the copies capability is correctly parsed for the max copies
  // supported.
  test('set copies max', async () => {
    const copiesInput =
        copiesSection.shadowRoot!
            .querySelector('print-preview-number-settings-section')!.getInput();
    assertEquals('1', copiesInput.value);
    assertFalse(copiesSection.getSetting('copies').setFromUi);

    copiesSection.capability = {max: 1234};
    await checkCopiesMax(1234);

    // Missing and empty capabilities should choose default max copies.
    copiesSection.capability = {};
    await checkCopiesMax(DEFAULT_MAX_COPIES);
  });

  test('collate visibility', async () => {
    const collateSection =
        copiesSection.shadowRoot!.querySelector<HTMLElement>('.checkbox')!;
    assertTrue(collateSection.hidden);

    copiesSection.setSetting('copies', 2);
    assertFalse(collateSection.hidden);

    model.set('settings.collate.available', false);
    assertTrue(collateSection.hidden);
    model.set('settings.collate.available', true);
    assertFalse(collateSection.hidden);

    // Set copies empty.
    const copiesInput =
        copiesSection.shadowRoot!
            .querySelector('print-preview-number-settings-section')!.getInput();
    await triggerInputEvent(copiesInput, '', copiesSection);
    assertTrue(collateSection.hidden);

    // Set copies valid again.
    await triggerInputEvent(copiesInput, '3', copiesSection);
    assertFalse(collateSection.hidden);

    // Set copies invalid.
    await triggerInputEvent(copiesInput, '0', copiesSection);
    assertTrue(collateSection.hidden);
  });

  // Verifies that setting the copies value using the number input works
  // correctly.
  test('set copies', async () => {
    const copiesInput =
        copiesSection.shadowRoot!
            .querySelector('print-preview-number-settings-section')!.getInput();
    assertEquals('1', copiesInput.value);
    assertFalse(copiesSection.getSetting('copies').setFromUi);

    await triggerInputEvent(copiesInput, '2', copiesSection);
    assertEquals(2, copiesSection.getSettingValue('copies'));
    assertTrue(copiesSection.getSetting('copies').valid);
    assertTrue(copiesSection.getSetting('copies').setFromUi);

    // Empty entry.
    await triggerInputEvent(copiesInput, '', copiesSection);
    assertEquals(2, copiesSection.getSettingValue('copies'));
    assertTrue(copiesSection.getSetting('copies').valid);

    // Invalid entry.
    await triggerInputEvent(copiesInput, '0', copiesSection);
    assertEquals(1, copiesSection.getSettingValue('copies'));
    assertFalse(copiesSection.getSetting('copies').valid);
  });

  // Verifies that setting the collate value using the checkbox works
  // correctly.
  test('set collate', function() {
    const collateCheckbox = copiesSection.$.collate;
    copiesSection.setSetting('copies', 2);
    assertTrue(collateCheckbox.checked);
    assertFalse(copiesSection.getSetting('collate').setFromUi);

    collateCheckbox.click();
    assertFalse(collateCheckbox.checked);
    collateCheckbox.dispatchEvent(new CustomEvent('change'));
    assertFalse(
        /** @type {boolean} */ (copiesSection.getSettingValue('collate')));
    assertTrue(copiesSection.getSetting('collate').setFromUi);
  });

  // Verifies that the inputs update when the value is updated.
  test('update from settings', function() {
    const copiesInput =
        copiesSection.shadowRoot!
            .querySelector('print-preview-number-settings-section')!.getInput();
    const collateCheckbox = copiesSection.$.collate;

    assertEquals('1', copiesInput.value);
    copiesSection.setSetting('copies', 3);
    assertEquals('3', copiesInput.value);

    assertTrue(collateCheckbox.checked);
    copiesSection.setSetting('collate', false);
    assertFalse(collateCheckbox.checked);
  });
});
