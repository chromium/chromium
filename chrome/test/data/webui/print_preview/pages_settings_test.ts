// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';

import type {PrintPreviewPagesSettingsElement, Range} from 'chrome://print/print_preview.js';
import {PagesValue} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyEventOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {selectOption, triggerInputEvent} from './print_preview_test_utils.js';

suite('PagesSettingsTest', function() {
  let pagesSection: PrintPreviewPagesSettingsElement;

  const oneToHundred: number[] = Array.from({length: 100}, (_x, i) => i + 1);

  const limitError: string = 'Out of bounds page reference, limit is ';

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    pagesSection = document.createElement('print-preview-pages-settings');
    pagesSection.settings = model.settings;
    pagesSection.disabled = false;
    fakeDataBind(model, pagesSection, 'settings');
    document.body.appendChild(pagesSection);
  });

  /**
   * @param inputString The input value to set.
   * @return Promise that resolves when the input has been set and
   *     the input-change event has fired.
   */
  function setCustomInput(inputString: string): Promise<void> {
    const pagesInput = pagesSection.$.pageSettingsCustomInput.inputElement;
    return triggerInputEvent(pagesInput, inputString, pagesSection);
  }

  /**
   * @param expectedPages The expected pages value.
   * @param expectedPages The expected pages value.
   * @param expectedError The expected error message.
   * @param invalid Whether the pages setting should be invalid.
   */
  function validateState(
      expectedPages: number[], expectedRanges: Range[], expectedError: string,
      invalid: boolean) {
    const pagesValue = pagesSection.getSettingValue('pages');
    assertEquals(expectedPages.length, pagesValue.length);
    expectedPages.forEach((page: number, index: number) => {
      assertEquals(page, pagesValue[index]);
    });
    const rangesValue = pagesSection.getSettingValue('ranges');
    assertEquals(expectedRanges.length, rangesValue.length);
    expectedRanges.forEach((range: Range, index: number) => {
      assertEquals(range.to, rangesValue[index].to);
      assertEquals(range.from, rangesValue[index].from);
    });
    assertEquals(!invalid, pagesSection.getSetting('pages').valid);
    assertEquals(
        expectedError !== '',
        pagesSection.shadowRoot!.querySelector('cr-input')!.invalid);
    assertEquals(
        expectedError,
        pagesSection.shadowRoot!.querySelector('cr-input')!.errorMessage);
  }

  // Verifies that the pages setting updates correctly when the dropdown
  // changes.
  test('PagesDropdown', async () => {
    pagesSection.pageCount = 5;

    // Default value is all pages.
    const customInputCollapse =
        pagesSection.shadowRoot!.querySelector('cr-collapse')!;

    assertFalse(pagesSection.getSetting('ranges').setFromUi);
    validateState([1, 2, 3, 4, 5], [], '', false);
    assertFalse(customInputCollapse.opened);

    // Set selection to odd pages.
    await selectOption(pagesSection, PagesValue.ODDS.toString());
    assertFalse(customInputCollapse.opened);
    validateState(
        [1, 3, 5], [{from: 1, to: 1}, {from: 3, to: 3}, {from: 5, to: 5}], '',
        false);

    // Set selection to even pages.
    await selectOption(pagesSection, PagesValue.EVENS.toString());
    assertFalse(customInputCollapse.opened);
    validateState([2, 4], [{from: 2, to: 2}, {from: 4, to: 4}], '', false);

    // Set selection of pages 1 and 2.
    await selectOption(pagesSection, PagesValue.CUSTOM.toString());
    assertTrue(customInputCollapse.opened);

    await setCustomInput('1-2');
    validateState([1, 2], [{from: 1, to: 2}], '', false);
    assertTrue(pagesSection.getSetting('ranges').setFromUi);

    // Re-select "all".
    await selectOption(pagesSection, PagesValue.ALL.toString());
    assertFalse(customInputCollapse.opened);
    validateState([1, 2, 3, 4, 5], [], '', false);

    // Re-select custom. The previously entered value should be
    // restored.
    await selectOption(pagesSection, PagesValue.CUSTOM.toString());
    assertTrue(customInputCollapse.opened);
    validateState([1, 2], [{from: 1, to: 2}], '', false);

    // Set a selection equal to the full page range. This should set ranges to
    // empty, so that reselecting "all" does not regenerate the preview.
    await setCustomInput('1-5');
    validateState([1, 2, 3, 4, 5], [], '', false);
  });

  // Tests that the odd-only and even-only options are hidden when the document
  // has only one page.
  test('NoParityOptions', async () => {
    pagesSection.pageCount = 1;

    const oddOption = pagesSection.shadowRoot!.querySelector<HTMLOptionElement>(
        `[value="${PagesValue.ODDS}"]`)!;
    assertTrue(oddOption.hidden);

    const evenOption =
        pagesSection.shadowRoot!.querySelector<HTMLOptionElement>(
            `[value="${PagesValue.EVENS}"]`)!;
    assertTrue(evenOption.hidden);
  });

  // Tests that the odd-only and even-only selections are preserved when the
  // page counts change.
  test('ParitySelectionMemorized', async () => {
    const select = pagesSection.shadowRoot!.querySelector('select')!;

    pagesSection.pageCount = 2;
    assertEquals(PagesValue.ALL.toString(), select.value);

    await selectOption(pagesSection, PagesValue.ODDS.toString());
    assertEquals(PagesValue.ODDS.toString(), select.value);

    let whenValueChanged =
        eventToPromise('process-select-change', pagesSection);
    pagesSection.pageCount = 1;
    await whenValueChanged;
    assertEquals(PagesValue.ALL.toString(), select.value);

    whenValueChanged = eventToPromise('process-select-change', pagesSection);
    pagesSection.pageCount = 2;
    await whenValueChanged;
    assertEquals(PagesValue.ODDS.toString(), select.value);
  });

  // Tests that the page ranges set are valid for different user inputs.
  test('ValidPageRanges', async () => {
    pagesSection.pageCount = 100;
    const tenToHundred = Array.from({length: 91}, (_x, i) => i + 10);

    await selectOption(pagesSection, PagesValue.CUSTOM.toString());
    await setCustomInput('1, 2, 3, 1, 56');
    validateState(
        [1, 2, 3, 56], [{from: 1, to: 3}, {from: 56, to: 56}], '', false);

    await setCustomInput('1-3, 6-9, 6-10');
    validateState(
        [1, 2, 3, 6, 7, 8, 9, 10], [{from: 1, to: 3}, {from: 6, to: 10}], '',
        false);

    await setCustomInput('10-');
    validateState(tenToHundred, [{from: 10, to: 100}], '', false);

    await setCustomInput('10-100');
    validateState(tenToHundred, [{from: 10, to: 100}], '', false);

    await setCustomInput('-');
    validateState(oneToHundred, [], '', false);

    // https://crbug.com/806165
    await setCustomInput('1\u30012\u30013\u30011\u300156');
    validateState(
        [1, 2, 3, 56], [{from: 1, to: 3}, {from: 56, to: 56}], '', false);

    await setCustomInput('1,2,3\u30011\u300156');
    validateState(
        [1, 2, 3, 56], [{from: 1, to: 3}, {from: 56, to: 56}], '', false);

    // https://crbug.com/1015145
    // Tests that the pages gets sorted for an unsorted input.
    await setCustomInput('89-91, 3, 6, 46, 1, 4, 2-3');
    validateState(
        [1, 2, 3, 4, 6, 46, 89, 90, 91],
        [
          {from: 1, to: 4},
          {from: 6, to: 6},
          {from: 46, to: 46},
          {from: 89, to: 91},
        ],
        '', false);
  });

  // Tests that the correct error messages are shown for different user
  // inputs.
  test('InvalidPageRanges', async () => {
    pagesSection.pageCount = 100;
    const syntaxError = 'Invalid page range, use e.g. 1-5, 8, 11-13';

    await selectOption(pagesSection, PagesValue.CUSTOM.toString());
    await setCustomInput('10-100000');
    validateState(oneToHundred, [], limitError + '100', true);

    await setCustomInput('1, 2, 0, 56');
    validateState(oneToHundred, [], syntaxError, true);

    await setCustomInput('-1, 1, 2,, 56');
    validateState(oneToHundred, [], syntaxError, true);

    await setCustomInput('1,2,56-40');
    validateState(oneToHundred, [], syntaxError, true);

    await setCustomInput('101-110');
    validateState(oneToHundred, [], limitError + '100', true);

    await setCustomInput('1\u30012\u30010\u300156');
    validateState(oneToHundred, [], syntaxError, true);

    await setCustomInput('-1,1,2\u3001\u300156');
    validateState(oneToHundred, [], syntaxError, true);

    await setCustomInput('--');
    validateState(oneToHundred, [], syntaxError, true);

    await setCustomInput(' 1 1 ');
    validateState(oneToHundred, [], syntaxError, true);
  });

  // Tests that the pages are set correctly for different values of pages per
  // sheet, and that ranges remain fixed (since they are used for generating
  // the print preview ticket).
  test('NupChangesPages', async () => {
    pagesSection.pageCount = 100;
    await selectOption(pagesSection, PagesValue.CUSTOM.toString());
    await setCustomInput('1, 2, 3, 1, 56');
    let expectedRanges = [{from: 1, to: 3}, {from: 56, to: 56}];
    validateState([1, 2, 3, 56], expectedRanges, '', false);
    pagesSection.setSetting('pagesPerSheet', 2);
    validateState([1, 2], expectedRanges, '', false);
    pagesSection.setSetting('pagesPerSheet', 4);
    validateState([1], expectedRanges, '', false);
    pagesSection.setSetting('pagesPerSheet', 1);

    await setCustomInput('1-3, 6-9, 6-10');
    expectedRanges = [{from: 1, to: 3}, {from: 6, to: 10}];
    validateState([1, 2, 3, 6, 7, 8, 9, 10], expectedRanges, '', false);
    pagesSection.setSetting('pagesPerSheet', 2);
    validateState([1, 2, 3, 4], expectedRanges, '', false);
    pagesSection.setSetting('pagesPerSheet', 3);
    validateState([1, 2, 3], expectedRanges, '', false);

    await setCustomInput('1-3');
    expectedRanges = [{from: 1, to: 3}];
    validateState([1], expectedRanges, '', false);
    pagesSection.setSetting('pagesPerSheet', 1);
    validateState([1, 2, 3], expectedRanges, '', false);
  });

  // Note: Remaining tests in this file are interactive_ui_tests, and validate
  // some focus related behavior.
  // Tests that the clearing a valid input has no effect, clearing an invalid
  // input does not show an error message but does not reset the preview, and
  // changing focus from an empty input in either case fills in the dropdown
  // with the full page range.
  test('ClearInput', async () => {
    pagesSection.pageCount = 3;
    const input = pagesSection.$.pageSettingsCustomInput.inputElement;
    const select = pagesSection.shadowRoot!.querySelector('select')!;
    const allValue = PagesValue.ALL.toString();
    const customValue = PagesValue.CUSTOM.toString();
    assertEquals(allValue, select.value);

    // Selecting custom focuses the input.
    await Promise.all([
      selectOption(pagesSection, customValue),
      eventToPromise('focus', input),
    ]);
    input.focus();

    await setCustomInput('1-2');
    assertEquals(customValue, select.value);
    validateState([1, 2], [{from: 1, to: 2}], '', false);

    await setCustomInput('');
    assertEquals(customValue, select.value);
    validateState([1, 2], [{from: 1, to: 2}], '', false);
    let whenBlurred =
        eventToPromise('custom-input-blurred-for-test', pagesSection);
    input.blur();

    await whenBlurred;
    await pagesSection.$.pageSettingsCustomInput.updateComplete;
    // Blurring a blank field sets the full page range.
    assertEquals(customValue, select.value);
    validateState([1, 2, 3], [], '', false);
    assertEquals('1-3', input.value);
    input.focus();

    await setCustomInput('5');
    assertEquals(customValue, select.value);
    // Invalid input doesn't change the preview.
    validateState([1, 2, 3], [], limitError + '3', true);

    await setCustomInput('');
    assertEquals(customValue, select.value);
    validateState([1, 2, 3], [], '', false);
    whenBlurred = eventToPromise('custom-input-blurred-for-test', pagesSection);
    input.blur();

    // Blurring an invalid value that has been cleared should reset the
    // value to all pages.
    await whenBlurred;
    assertEquals(customValue, select.value);
    validateState([1, 2, 3], [], '', false);
    assertEquals('1-3', input.value);

    // Re-focus and clear the input and then select "All" in the
    // dropdown.
    input.focus();

    await setCustomInput('');
    select.focus();

    await selectOption(pagesSection, allValue);
    flush();
    assertEquals(allValue, select.value);
    validateState([1, 2, 3], [], '', false);

    // Reselect custom. This should focus the input.
    await Promise.all([
      selectOption(pagesSection, customValue),
      eventToPromise('focus', input),
    ]);
    // Input has been cleared.
    assertEquals('', input.value);
    validateState([1, 2, 3], [], '', false);
    whenBlurred = eventToPromise('custom-input-blurred-for-test', pagesSection);
    input.blur();

    await whenBlurred;
    assertEquals('1-3', input.value);

    // Change the page count. Since the range was set automatically, this
    // should reset it to the new set of all pages.
    pagesSection.pageCount = 2;
    await pagesSection.$.pageSettingsCustomInput.updateComplete;
    validateState([1, 2], [], '', false);
    assertEquals('1-2', input.value);
  });

  // Verifies that the input is never disabled when the validity of the
  // setting changes.
  test(
      'InputNotDisabledOnValidityChange', async () => {
        pagesSection.pageCount = 3;
        // In the real UI, the print preview app listens for this event from
        // this section and others and sets disabled to true if any change from
        // true to false is detected. Imitate this here. Since we are only
        // interacting with the pages input, at no point should the input be
        // disabled, as it will lose focus.
        pagesSection.addEventListener('setting-valid-changed', function(e) {
          assertFalse(pagesSection.$.pageSettingsCustomInput.disabled);
          pagesSection.set('disabled', !(e as CustomEvent<boolean>).detail);
          assertFalse(pagesSection.$.pageSettingsCustomInput.disabled);
        });

        await selectOption(pagesSection, PagesValue.CUSTOM.toString());
        await setCustomInput('1');
        validateState([1], [{from: 1, to: 1}], '', false);

        await setCustomInput('12');
        validateState([1], [{from: 1, to: 1}], limitError + '3', true);

        // Restore valid input
        await setCustomInput('1');
        validateState([1], [{from: 1, to: 1}], '', false);

        // Invalid input again
        await setCustomInput('8');
        validateState([1], [{from: 1, to: 1}], limitError + '3', true);

        // Clear input
        await setCustomInput('');
        validateState([1], [{from: 1, to: 1}], '', false);

        // Set valid input
        await setCustomInput('2');
        validateState([2], [{from: 2, to: 2}], '', false);
      });

  // Verifies that the enter key event is bubbled to the pages settings
  // element, so that it will be bubbled to the print preview app to trigger a
  // print.
  test(
      'EnterOnInputTriggersPrint', async () => {
        pagesSection.pageCount = 3;
        const input = pagesSection.$.pageSettingsCustomInput.inputElement;
        const whenPrintReceived = eventToPromise('keydown', pagesSection);

        // Setup an empty input by selecting custom..
        const customValue = PagesValue.CUSTOM.toString();
        const pagesSelect = pagesSection.shadowRoot!.querySelector('select')!;
        await Promise.all([
          selectOption(pagesSection, customValue),
          eventToPromise('focus', input),
        ]);
        assertEquals(customValue, pagesSelect.value);
        keyEventOn(input, 'keydown', 0, [], 'Enter');

        await whenPrintReceived;
        // Keep custom selected, but pages to print should still be all.
        assertEquals(customValue, pagesSelect.value);
        validateState([1, 2, 3], [], '', false);

        // Select a custom input of 1.
        await setCustomInput('1');
        assertEquals(customValue, pagesSelect.value);
        const whenSecondPrintReceived = eventToPromise('keydown', pagesSection);
        keyEventOn(input, 'keydown', 0, [], 'Enter');

        await whenSecondPrintReceived;
        assertEquals(customValue, pagesSelect.value);
        validateState([1], [{from: 1, to: 1}], '', false);
      });
});
