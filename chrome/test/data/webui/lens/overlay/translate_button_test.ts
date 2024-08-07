// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/translate_button.js';

import type {TranslateButtonElement} from 'chrome-untrusted://lens/translate_button.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

suite('OverlayTranslateButton', function() {
  let overlayTranslateButtonElement: TranslateButtonElement;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    overlayTranslateButtonElement = document.createElement('translate-button');
    document.body.appendChild(overlayTranslateButtonElement);
  });

  test('LanguagePickerIsVisible', () => {
    assertFalse(isVisible(overlayTranslateButtonElement.$.languagePicker));

    // Click the translate button to show the language picker.
    overlayTranslateButtonElement.$.translateButton.click();

    assertTrue(isVisible(overlayTranslateButtonElement.$.languagePicker));

    // Clicking again should toggle the language picker.
    overlayTranslateButtonElement.$.translateButton.click();

    assertFalse(isVisible(overlayTranslateButtonElement.$.languagePicker));
  });
});
