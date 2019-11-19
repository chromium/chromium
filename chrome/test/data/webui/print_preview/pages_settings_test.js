// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {keyEventOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {selectOption, triggerInputEvent} from 'chrome://test/print_preview/print_preview_test_utils.js';
import {eventToPromise, fakeDataBind} from 'chrome://test/test_util.m.js';

window.pages_settings_test = {};
pages_settings_test.suiteName = 'PagesSettingsTest';
/** @enum {string} */
pages_settings_test.TestNames = {
  PagesDropdown: 'pages dropdown',
  ValidPageRanges: 'valid page ranges',
  InvalidPageRanges: 'invalid page ranges',
  NupChangesPages: 'nup changes pages',
  ClearInput: 'clear input',
  InputNotDisabledOnValidityChange: 'input not disabled on validity change',
  EnterOnInputTriggersPrint: 'enter on input triggers print',
};

suite(pages_settings_test.suiteName, function() {
  /** @type {?PrintPreviewPagesSettingsElement} */
  let pagesSection = null;

  /** @type {!Array<number>} */
  const oneToHundred = Array.from({length: 100}, (x, i) => i + 1);

  /** @type {string} */
  const limitError = 'Out of bounds page reference, limit is ';

  /** @override */
  setup(function() {
    PolymerTest.clearBody();
    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    pagesSection = document.createElement('print-preview-pages-settings');
    pagesSection.settings = model.settings;
    pagesSection.disabled = false;
    fakeDataBind(model, pagesSection, 'settings');
    document.body.appendChild(pagesSection);
  });

  /**
   * @param {string} inputString The input value to set.
   * @return {!Promise} Promise that resolves when the input has been set and
   *     the input-change event has fired.
   */
  function setCustomInput(inputString) {
    const pagesInput = pagesSection.$.pageSettingsCustomInput.inputElement;
    return triggerInputEvent(pagesInput, inputString, pagesSection);
  }

  /**
   * @param {!Array<number>} expectedPages The expected pages value.
   * @param {!Array<{to: number, from: number>} expectedPages The expected
   *     pages value.
   * @param {string} expectedError The expected error message.
   * @param {boolean} invalid Whether the pages setting should be invalid.
   */
  function validateState(
      expectedPages, expectedRanges, expectedError, invalid) {
    const pagesValue = pagesSection.getSettingValue('pages');
    assertEquals(expectedPages.length, pagesValue.length);
    expectedPages.forEach((page, index) => {
      assertEquals(page, pagesValue[index]);
    });
    const rangesValue = pagesSection.getSettingValue('ranges');
    assertEquals(expectedRanges.length, rangesValue.length);
    expectedRanges.forEach((range, index) => {
      assertEquals(range.to, rangesValue[index].to);
      assertEquals(range.from, rangesValue[index].from);
    });
    assertEquals(!invalid, pagesSection.getSetting('pages').valid);
    assertEquals(expectedError !== '', pagesSection.$$('cr-input').invalid);
    assertEquals(expectedError, pagesSection.$$('cr-input').errorMessage);
  }

  // Verifies that the pages setting updates correctly when the dropdown
  // changes.
  test(assert(pages_settings_test.TestNames.PagesDropdown), async () => {
    pagesSection.pageCount = 3;

    // Default value is all pages.
    const pagesSelect = pagesSection.$$('select');
    const customInputCollapse = pagesSection.$$('iron-collapse');
    const pagesCrInput = pagesSection.$.pageSettingsCustomInput;
    const pagesInput = pagesCrInput.inputElement;

    assertFalse(pagesSection.getSetting('ranges').setFromUi);
    validateState([1, 2, 3], [], '', false);
    assertFalse(customInputCollapse.opened);

    // Set selection of pages 1 and 2.
    await selectOption(
        pagesSection, pagesSection.pagesValueEnum_.CUSTOM.toString());
    assertTrue(customInputCollapse.opened);

    await setCustomInput('1-2');
    validateState([1, 2], [{from: 1, to: 2}], '', false);
    assertTrue(pagesSection.getSetting('ranges').setFromUi);

    // Re-select "all".
    await selectOption(
        pagesSection, pagesSection.pagesValueEnum_.ALL.toString());
    assertFalse(customInputCollapse.opened);
    validateState([1, 2, 3], [], '', false);

    // Re-select custom. The previously entered value should be
    // restored.
    await selectOption(
        pagesSection, pagesSection.pagesValueEnum_.CUSTOM.toString());
    assertTrue(customInputCollapse.opened);
    validateState([1, 2], [{from: 1, to: 2}], '', false);

    // Set a selection equal to the full page range. This should set ranges to
    // empty, so that reselecting "all" does not regenerate the preview.
    await setCustomInput('1-3');
    validateState([1, 2, 3], [], '', false);
  });

  // Tests that the page ranges set are valid for different user inputs.
  test(assert(pages_settings_test.TestNames.ValidPageRanges), async () => {
    pagesSection.pageCount = 100;
    const tenToHundred = Array.from({length: 91}, (x, i) => i + 10);

    await selectOption(
        pagesSection, pagesSection.pagesValueEnum_.CUSTOM.toString());
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
          {from: 1, to: 4}, {from: 6, to: 6}, {from: 46, to: 46},
          {from: 89, to: 91}
        ],
        '', false);
  });

  // Tests that the correct error messages are shown for different user
  // inputs.
  test(assert(pages_settings_test.TestNames.InvalidPageRanges), async () => {
    pagesSection.pageCount = 100;
    const syntaxError = 'Invalid page range, use e.g. 1-5, 8, 11-13';

    await selectOption(
        pagesSection, pagesSection.pagesValueEnum_.CUSTOM.toString());
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
  test(assert(pages_settings_test.TestNames.NupChangesPages), async () => {
    pagesSection.pageCount = 100;
    await selectOption(
        pagesSection, pagesSection.pagesValueEnum_.CUSTOM.toString());
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
  test(assert(pages_settings_test.TestNames.ClearInput), async () => {
    pagesSection.pageCount = 3;
    const input = pagesSection.$.pageSettingsCustomInput.inputElement;
    const select = pagesSection.$$('select');
    const allValue = pagesSection.pagesValueEnum_.ALL.toString();
    const customValue = pagesSection.pagesValueEnum_.CUSTOM.toString();
    assertEquals(allValue, select.value);

    // Selecting custom focuses the input.
    await Promise.all([
      selectOption(pagesSection, customValue), eventToPromise('focus', input)
    ]);
    input.focus();

    await setCustomInput('1-2');
    assertEquals(customValue, select.value);
    validateState([1, 2], [{from: 1, to: 2}], '', false);

    await setCustomInput('');
    assertEquals(customValue, select.value);
    validateState([1, 2], [{from: 1, to: 2}], '', false);
    let whenBlurred = eventToPromise('blur', input);
    input.blur();

    await whenBlurred;
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
    whenBlurred = eventToPromise('blur', input);
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

    await setCustomInput('', 3);
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
    whenBlurred = eventToPromise('blur', input);
    input.blur();

    await whenBlurred;
    assertEquals('1-3', input.value);

    // Change the page count. Since the range was set automatically, this
    // should reset it to the new set of all pages.
    pagesSection.pageCount = 2;
    validateState([1, 2], [], '', false);
    assertEquals('1-2', input.value);
  });

  // Verifies that the input is never disabled when the validity of the
  // setting changes.
  test(
      assert(pages_settings_test.TestNames.InputNotDisabledOnValidityChange),
      async () => {
        pagesSection.pageCount = 3;
        // In the real UI, the print preview app listens for this event from
        // this section and others and sets disabled to true if any change from
        // true to false is detected. Imitate this here. Since we are only
        // interacting with the pages input, at no point should the input be
        // disabled, as it will lose focus.
        pagesSection.addEventListener('setting-valid-changed', function(e) {
          assertFalse(pagesSection.$.pageSettingsCustomInput.disabled);
          pagesSection.set('disabled', !e.detail);
          assertFalse(pagesSection.$.pageSettingsCustomInput.disabled);
        });

        const input = pagesSection.$.pageSettingsCustomInput.inputElement;
        await selectOption(
            pagesSection, pagesSection.pagesValueEnum_.CUSTOM.toString());
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
      assert(pages_settings_test.TestNames.EnterOnInputTriggersPrint),
      async () => {
        pagesSection.pageCount = 3;
        const input = pagesSection.$.pageSettingsCustomInput.inputElement;
        const whenPrintReceived = eventToPromise('keydown', pagesSection);

        // Setup an empty input by selecting custom..
        const customValue = pagesSection.pagesValueEnum_.CUSTOM.toString();
        const pagesSelect = pagesSection.$$('select');
        await Promise.all([
          selectOption(pagesSection, customValue),
          eventToPromise('focus', input)
        ]);
        assertEquals(customValue, pagesSelect.value);
        keyEventOn(input, 'keydown', 13, [], 'Enter');

        await whenPrintReceived;
        // Keep custom selected, but pages to print should still be all.
        assertEquals(customValue, pagesSelect.value);
        validateState([1, 2, 3], [], '', false);

        // Select a custom input of 1.
        await setCustomInput('1');
        assertEquals(customValue, pagesSelect.value);
        const whenSecondPrintReceived = eventToPromise('keydown', pagesSection);
        keyEventOn(input, 'keydown', 13, [], 'Enter');

        await whenSecondPrintReceived;
        assertEquals(customValue, pagesSelect.value);
        validateState([1], [{from: 1, to: 1}], '', false);
      });
});
