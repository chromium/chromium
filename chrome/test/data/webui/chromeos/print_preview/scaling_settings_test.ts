// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrintPreviewModelElement, PrintPreviewScalingSettingsElement} from 'chrome://print/print_preview.js';
import {ScalingType} from 'chrome://print/print_preview.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {selectOption, triggerInputEvent} from './print_preview_test_utils.js';

suite('ScalingSettingsTest', function() {
  let scalingSection: PrintPreviewScalingSettingsElement;

  let model: PrintPreviewModelElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    scalingSection = document.createElement('print-preview-scaling-settings');
    scalingSection.settings = model.settings;
    scalingSection.disabled = false;
    setDocumentPdf(false);
    fakeDataBind(model, scalingSection, 'settings');
    document.body.appendChild(scalingSection);
  });

  test(
      'ShowCorrectDropdownOptions', function() {
        // Not a PDF document -> No fit to page or fit to paper options.
        const fitToPageOption =
            scalingSection.shadowRoot!.querySelector<HTMLOptionElement>(
                `[value="${ScalingType.FIT_TO_PAGE}"]`)!;
        const fitToPaperOption =
            scalingSection.shadowRoot!.querySelector<HTMLOptionElement>(
                `[value="${ScalingType.FIT_TO_PAPER}"]`)!;
        const defaultOption =
            scalingSection.shadowRoot!.querySelector<HTMLOptionElement>(
                `[value="${ScalingType.DEFAULT}"]`)!;
        const customOption =
            scalingSection.shadowRoot!.querySelector<HTMLOptionElement>(
                `[value="${ScalingType.CUSTOM}"]`)!;
        assertTrue(fitToPageOption.hidden && fitToPageOption.disabled);
        assertTrue(fitToPaperOption.hidden && fitToPaperOption.disabled);
        assertFalse(defaultOption.hidden && !defaultOption.disabled);
        assertFalse(customOption.hidden && !customOption.disabled);

        // Fit to page and paper available -> All 4 options.
        setDocumentPdf(true);
        assertFalse(fitToPageOption.hidden && !fitToPageOption.disabled);
        assertFalse(fitToPaperOption.hidden && !fitToPaperOption.disabled);
        assertFalse(defaultOption.hidden && !defaultOption.disabled);
        assertFalse(customOption.hidden && !customOption.disabled);
      });

  /**
   * @param expectedScaling The expected scaling value.
   * @param valid Whether the scaling setting is valid.
   * @param scalingType Expected scaling type for modifiable content.
   * @param scalingTypePdf Expected scaling type for PDFs.
   * @param scalingDisplayValue The value that should be displayed in
   *     the UI for scaling.
   */
  function validateState(
      expectedScaling: string, valid: boolean, scalingType: ScalingType,
      scalingTypePdf: ScalingType, scalingDisplayValue: string) {
    // Validate the settings were set as expected.
    assertEquals(expectedScaling, scalingSection.getSettingValue('scaling'));
    assertEquals(valid, scalingSection.getSetting('scaling').valid);
    assertEquals(scalingType, scalingSection.getSetting('scalingType').value);
    assertEquals(
        scalingTypePdf, scalingSection.getSetting('scalingTypePdf').value);

    // Validate UI values that are set by JS.
    const scalingInput =
        scalingSection.shadowRoot!
            .querySelector('print-preview-number-settings-section')!.getInput();
    const expectedCollapseOpened =
        (scalingSection.getSettingValue('scalingType') ===
         ScalingType.CUSTOM) ||
        (scalingSection.getSettingValue('scalingTypePdf') ===
         ScalingType.CUSTOM);
    const collapse = scalingSection.shadowRoot!.querySelector('cr-collapse')!;
    assertEquals(!valid, scalingInput.invalid);
    assertEquals(scalingDisplayValue, scalingInput.value);
    assertEquals(expectedCollapseOpened, collapse.opened);
  }

  /**
   * @param isPdf Whether the document is a PDF
   */
  function setDocumentPdf(isPdf: boolean) {
    model.set('settings.scalingType.available', !isPdf);
    model.set('settings.scalingTypePdf.available', isPdf);
    scalingSection.isPdf = isPdf;
  }

  // Verifies that setting the scaling value using the dropdown and/or the
  // custom input works correctly.
  test('SetScaling', async () => {
    // Default is 100
    const scalingCrInput =
        scalingSection.shadowRoot!
            .querySelector(
                'print-preview-number-settings-section')!.$.userValue;
    const scalingInput = scalingCrInput.inputElement;
    // Make fit to page and fit to paper available.
    setDocumentPdf(true);

    // Default is 100
    await scalingCrInput.updateComplete;
    validateState('100', true, ScalingType.DEFAULT, ScalingType.DEFAULT, '100');
    assertFalse(scalingSection.getSetting('scaling').setFromUi);
    assertFalse(scalingSection.getSetting('scalingType').setFromUi);
    assertFalse(scalingSection.getSetting('scalingTypePdf').setFromUi);

    // Select custom
    await selectOption(scalingSection, ScalingType.CUSTOM.toString());
    validateState('100', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '100');
    assertTrue(scalingSection.getSetting('scalingType').setFromUi);
    assertTrue(scalingSection.getSetting('scalingTypePdf').setFromUi);

    await triggerInputEvent(scalingInput, '105', scalingSection);
    validateState('105', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '105');
    assertTrue(scalingSection.getSetting('scaling').setFromUi);

    // Change to fit to page.
    await selectOption(scalingSection, ScalingType.FIT_TO_PAGE.toString());
    validateState(
        '105', true, ScalingType.CUSTOM, ScalingType.FIT_TO_PAGE, '105');

    // Change to fit to paper.
    await selectOption(scalingSection, ScalingType.FIT_TO_PAPER.toString());
    validateState(
        '105', true, ScalingType.CUSTOM, ScalingType.FIT_TO_PAPER, '105');

    // Go back to custom. Restores 105 value.
    await selectOption(scalingSection, ScalingType.CUSTOM.toString());
    validateState('105', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '105');

    // Set scaling to something invalid. Should change setting validity
    // but not value.
    await triggerInputEvent(scalingInput, '5', scalingSection);
    validateState('105', false, ScalingType.CUSTOM, ScalingType.CUSTOM, '5');

    // Select fit to page. Should clear the invalid value.
    await selectOption(scalingSection, ScalingType.FIT_TO_PAGE.toString());
    await scalingCrInput.updateComplete;
    validateState(
        '105', true, ScalingType.CUSTOM, ScalingType.FIT_TO_PAGE, '105');

    // Custom scaling should set to last valid.
    await selectOption(scalingSection, ScalingType.CUSTOM.toString());
    validateState('105', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '105');

    // Set scaling to something invalid. Should change setting validity
    // but not value.
    await triggerInputEvent(scalingInput, '500', scalingSection);
    validateState('105', false, ScalingType.CUSTOM, ScalingType.CUSTOM, '500');

    // Pick default scaling. This should clear the error.
    await selectOption(scalingSection, ScalingType.DEFAULT.toString());
    await scalingCrInput.updateComplete;
    validateState('105', true, ScalingType.DEFAULT, ScalingType.DEFAULT, '105');

    // Custom scaling should set to last valid.
    await selectOption(scalingSection, ScalingType.CUSTOM.toString());
    validateState('105', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '105');

    // Enter a blank value in the scaling field. This should not
    // change the stored value of scaling or scaling type, to avoid an
    // unnecessary preview regeneration.
    await triggerInputEvent(scalingInput, '', scalingSection);
    validateState('105', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '');
  });


  // Verifies that the input is never disabled when the validity of the
  // setting changes.
  test(
      'InputNotDisabledOnValidityChange', async () => {
        const numberSection = scalingSection.shadowRoot!.querySelector(
            'print-preview-number-settings-section')!;
        const input = numberSection.getInput();

        // In the real UI, the print preview app listens for this event from
        // this section and others and sets disabled to true if any change from
        // true to false is detected. Imitate this here. Since we are only
        // interacting with the scaling input, at no point should the input be
        // disabled, as it will lose focus.
        model.addEventListener('setting-valid-changed', function() {
          assertFalse(input.disabled);
        });

        await selectOption(scalingSection, ScalingType.CUSTOM.toString());
        await input.updateComplete;
        await triggerInputEvent(input, '90', scalingSection);
        validateState('90', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '90');

        // Set invalid input
        await triggerInputEvent(input, '9', scalingSection);
        validateState('90', false, ScalingType.CUSTOM, ScalingType.CUSTOM, '9');

        // Restore valid input
        await triggerInputEvent(input, '90', scalingSection);
        validateState('90', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '90');

        // Invalid input again
        await triggerInputEvent(input, '9', scalingSection);
        validateState('90', false, ScalingType.CUSTOM, ScalingType.CUSTOM, '9');

        // Clear input
        await triggerInputEvent(input, '', scalingSection);
        validateState('90', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '');

        // Set valid input
        await triggerInputEvent(input, '50', scalingSection);
        validateState('50', true, ScalingType.CUSTOM, ScalingType.CUSTOM, '50');
      });
});
