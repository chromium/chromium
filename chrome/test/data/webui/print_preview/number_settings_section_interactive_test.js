// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {triggerInputEvent} from 'chrome://test/print_preview/print_preview_test_utils.js';

window.number_settings_section_interactive_test = {};
number_settings_section_interactive_test.suiteName =
    'NumberSettingsSectionInteractiveTest';
/** @enum {string} */
number_settings_section_interactive_test.TestNames = {
  BlurResetsEmptyInput: 'blur resets empty input',
};

suite(number_settings_section_interactive_test.suiteName, function() {
  /** @type {?PrintPreviewNumberSettingsSectionElement} */
  let numberSettings = null;

  /** @override */
  setup(function() {
    PolymerTest.clearBody();

    document.body.innerHTML = `
          <print-preview-number-settings-section id="numberSettings"
              min-value="1" max-value="100" default-value="50"
              current-value="10" hint-message="incorrect value entered"
              input-valid>
          </print-preview-number-settings-section>`;
    numberSettings = document.querySelector('#numberSettings');
  });

  // Verifies that blurring the input will reset it to the default if it is
  // empty, but not if it contains an invalid value.
  test(
      assert(number_settings_section_interactive_test.TestNames
                 .BlurResetsEmptyInput),
      async () => {
        // Initial value is 10.
        const crInput = numberSettings.getInput();
        const input = crInput.inputElement;
        assertEquals('10', input.value);

        // Set something invalid in the input.
        input.focus();
        await triggerInputEvent(input, '0', numberSettings);
        assertEquals('0', input.value);
        assertTrue(crInput.invalid);

        // Blurring the input does not clear it or clear the error if there
        // is an explicit invalid value.
        input.blur();
        assertEquals('0', input.value);
        assertTrue(crInput.invalid);

        // Clear the input.
        input.focus();

        await triggerInputEvent(input, '', numberSettings);
        assertEquals('', input.value);
        assertFalse(crInput.invalid);

        // Blurring the input clears it to the default when it is empty.
        input.blur();
        assertEquals('50', input.value);
        assertFalse(crInput.invalid);
      });
});
