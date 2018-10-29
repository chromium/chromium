// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('number_settings_section_interactive_test', function() {
  /** @enum {string} */
  const TestNames = {
    BlurResetsEmptyInput: 'blur resets empty input',
  };

  const suiteName = 'NumberSettingsSectionInteractiveTest';
  suite(suiteName, function() {
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
    test(assert(TestNames.BlurResetsEmptyInput), function() {
      // Initial value is 10.
      const crInput = numberSettings.getInput();
      const input = crInput.inputElement;
      assertEquals('10', input.value);

      // Set something invalid in the input.
      const whenFocused = test_util.eventToPromise('focus', input);
      input.focus();
      print_preview_test_utils.triggerInputEvent(input, '0');
      return test_util.eventToPromise('input-change', numberSettings)
          .then(() => {
            assertEquals('0', input.value);
            assertTrue(crInput.invalid);

            // Blurring the input does not clear it or clear the error if there
            // is an explicit invalid value.
            input.blur();
            assertEquals('0', input.value);
            assertTrue(crInput.invalid);

            // Clear the input.
            input.focus();
            print_preview_test_utils.triggerInputEvent(input, '');
            return test_util.eventToPromise('input-change', numberSettings);
          })
          .then(() => {
            assertEquals('', input.value);
            assertFalse(crInput.invalid);

            // Blurring the input clears it to the default when it is empty.
            input.blur();
            assertEquals('50', input.value);
            assertFalse(crInput.invalid);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
