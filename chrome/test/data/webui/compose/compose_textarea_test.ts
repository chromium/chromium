// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://compose/textarea.js';

import type {ComposeTextareaElement} from 'chrome-untrusted://compose/textarea.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome-untrusted://webui-test/test_util.js';

suite('ComposeTextarea', () => {
  let textarea: ComposeTextareaElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    textarea = document.createElement('compose-textarea');
    textarea.inputParams = {
      minWordLimit: 5,
      maxWordLimit: 50,
      maxCharacterLimit: 100,
    };
    document.body.appendChild(textarea);
  });

  test('TogglesModes', () => {
    assertTrue(isVisible(textarea.$.input));
    textarea.value = 'Some text';  // Text to make sure elements are not empty.

    textarea.readonly = true;
    assertFalse(isVisible(textarea.$.input));
    assertTrue(isVisible(textarea.$.readonlyText));
    assertFalse(isVisible(textarea.$.editButtonContainer));

    textarea.allowExitingReadonlyMode = true;
    assertFalse(isVisible(textarea.$.input));
    assertTrue(isVisible(textarea.$.readonlyText));
    assertTrue(isVisible(textarea.$.editButtonContainer));
  });

  test('PassesInValue', () => {
    textarea.value = 'Here is my value.';
    assertEquals('Here is my value.', textarea.$.input.value);
    assertEquals(
        'Here is my value.', textarea.$.readonlyText.textContent!.trim());
  });

  test('NotifiesChangesToValue', async () => {
    const whenValueChanged = eventToPromise('value-changed', textarea);
    textarea.$.input.value = 'My new value';
    textarea.$.input.dispatchEvent(new Event('input'));
    await whenValueChanged;
    assertEquals('My new value', textarea.value);
  });

  test('Validates', () => {
    // No input yet, so should be invalid.
    assertFalse(textarea.validate());

    // Has at least 5 words, should be valid.
    textarea.$.input.value = 'Here is some input with more than 5 words.';
    assertTrue(textarea.validate());

    // Too short of an input, should be invalid and display an error.
    textarea.$.input.value = 'Short';
    assertFalse(textarea.validate());
    assertTrue(isVisible(textarea.$.tooShortError));

    // Too many characters, should show error.
    textarea.$.input.value = Array(101).fill('a').join('');
    assertFalse(textarea.validate());
    assertTrue(isVisible(textarea.$.tooLongError));

    // Should revalidate when value becomes valid.
    textarea.$.input.value = 'Here is another input with more than 5 words.';
    assertTrue(textarea.validate());

    // Too many words, should show error.
    textarea.$.input.value = Array(51).fill('a').join(' ');
    assertFalse(textarea.validate());
    assertTrue(isVisible(textarea.$.tooLongError));
  });
});
