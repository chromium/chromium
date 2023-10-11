// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compose/textarea.js';

import {ComposeTextareaElement} from 'chrome://compose/textarea.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('ComposeTextarea', () => {
  let textarea: ComposeTextareaElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    textarea = document.createElement('compose-textarea');
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

    // Has some input, should be valid.
    textarea.$.input.value = 'Here is some input.';
    assertTrue(textarea.validate());

    // Too short of an input, should be invalid.
    textarea.$.input.value = 'Short';
    assertFalse(textarea.validate());
    assertFalse(isVisible(textarea.$.error));

    // Too long of an input, should show error.
    textarea.$.input.value = Array(300).join('a');
    assertFalse(textarea.validate());
    assertTrue(isVisible(textarea.$.error));
  });
});
