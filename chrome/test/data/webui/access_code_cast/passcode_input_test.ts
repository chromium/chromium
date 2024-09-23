// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://access-code-cast/passcode_input/passcode_input.js';

import type {PasscodeInputElement} from 'chrome://access-code-cast/passcode_input/passcode_input.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('PasscodeInputElementTest', () => {
  let c2cInput: PasscodeInputElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    c2cInput = document.createElement('c2c-passcode-input');
    c2cInput.length = 6;
    document.body.appendChild(c2cInput);

    await waitAfterNextRender(c2cInput);
  });

  test('value set correctly', () => {
    const testValue = 'hello';
    c2cInput.value = testValue;
    assertEquals(c2cInput.$.inputElement.value, testValue);
  });

  test('focus shown correctly', async () => {
    c2cInput.value = '';
    c2cInput.focusInput();
    await waitAfterNextRender(c2cInput);

    assertTrue(c2cInput.focused);
    assertTrue(c2cInput.getCharBox(0).classList.contains('focused'));
    assertTrue(c2cInput.getCharBox(1).classList.contains('focused'));
    assertTrue(c2cInput.getCharBox(2).classList.contains('focused'));
  });

  test('cursor is rendered correctly', async () => {
    const testValue = 'test';
    c2cInput.value = testValue;
    c2cInput.focusInput();

    // Case 1: Cursor is after all text.
    c2cInput.$.inputElement
        .setSelectionRange(testValue.length, testValue.length);
    c2cInput.$.inputElement.dispatchEvent(new Event('select'));
    await waitAfterNextRender(c2cInput);

    assertTrue(c2cInput.getDisplayChar(testValue.length).classList
        .contains('cursor-empty'));
    assertFalse(c2cInput.getDisplayChar(testValue.length).classList
        .contains('cursor-start'));
    assertFalse(c2cInput.getDisplayChar(testValue.length).classList
        .contains('cursor-filled'));
    assertFalse(c2cInput.getDisplayChar(testValue.length - 1).classList
        .contains('cursor-empty'));
    assertFalse(c2cInput.getDisplayChar(testValue.length - 1).classList
        .contains('cursor-start'));
    assertFalse(c2cInput.getDisplayChar(testValue.length - 1).classList
        .contains('cursor-filled'));
    assertFalse(c2cInput.getDisplayChar(testValue.length + 1).classList
        .contains('cursor-empty'));
    assertFalse(c2cInput.getDisplayChar(testValue.length + 1).classList
        .contains('cursor-start'));
    assertFalse(c2cInput.getDisplayChar(testValue.length + 1).classList
        .contains('cursor-filled'));

    // Case 2: Cursor is before all text
    c2cInput.$.inputElement.setSelectionRange(0, 0);
    c2cInput.$.inputElement.dispatchEvent(new Event('select'));
    await waitAfterNextRender(c2cInput);

    assertFalse(c2cInput.getDisplayChar(0).classList
        .contains('cursor-empty'));
    assertTrue(c2cInput.getDisplayChar(0).classList
        .contains('cursor-start'));
    assertFalse(c2cInput.getDisplayChar(0).classList
        .contains('cursor-filled'));
    assertFalse(c2cInput.getDisplayChar(1).classList
        .contains('cursor-empty'));
    assertFalse(c2cInput.getDisplayChar(1).classList
        .contains('cursor-start'));
    assertFalse(c2cInput.getDisplayChar(1).classList
        .contains('cursor-filled'));
    assertFalse(c2cInput.getDisplayChar(2).classList
        .contains('cursor-empty'));
    assertFalse(c2cInput.getDisplayChar(2).classList
        .contains('cursor-start'));
    assertFalse(c2cInput.getDisplayChar(2).classList
        .contains('cursor-filled'));

    // Case 3: Cursor is between characters of text
    c2cInput.$.inputElement.setSelectionRange(1, 1);
    c2cInput.$.inputElement.dispatchEvent(new Event('select'));
    await waitAfterNextRender(c2cInput);

    assertFalse(c2cInput.getDisplayChar(0).classList
        .contains('cursor-empty'));
    assertFalse(c2cInput.getDisplayChar(0).classList
        .contains('cursor-start'));
    assertTrue(c2cInput.getDisplayChar(0).classList
        .contains('cursor-filled'));
    assertFalse(c2cInput.getDisplayChar(1).classList
        .contains('cursor-empty'));
    assertFalse(c2cInput.getDisplayChar(1).classList
        .contains('cursor-start'));
    assertFalse(c2cInput.getDisplayChar(1).classList
        .contains('cursor-filled'));
    assertFalse(c2cInput.getDisplayChar(2).classList
        .contains('cursor-empty'));
    assertFalse(c2cInput.getDisplayChar(2).classList
        .contains('cursor-start'));
    assertFalse(c2cInput.getDisplayChar(2).classList
        .contains('cursor-filled'));
  });

  test('disabled state propogates correctly', async () => {
    c2cInput.value = '';
    c2cInput.disabled = false;
    await waitAfterNextRender(c2cInput);

    assertFalse(c2cInput.$.inputElement.disabled);
    assertFalse(c2cInput.getDisplayChar(0).classList.contains('disabled'));
    assertFalse(c2cInput.getDisplayChar(1).classList.contains('disabled'));
    assertFalse(c2cInput.getDisplayChar(2).classList.contains('disabled'));

    c2cInput.disabled = true;
    await waitAfterNextRender(c2cInput);

    assertTrue(c2cInput.$.inputElement.disabled);
    assertTrue(c2cInput.getDisplayChar(0).classList.contains('disabled'));
    assertTrue(c2cInput.getDisplayChar(1).classList.contains('disabled'));
    assertTrue(c2cInput.getDisplayChar(2).classList.contains('disabled'));
  });
});
