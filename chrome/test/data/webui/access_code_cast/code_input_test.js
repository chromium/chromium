// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://access-code-cast/code_input/code_input.js';

import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

suite('CodeInputElementTest', () => {
  /** @type {!CodeInputElement} */
  let c2cInput;

  setup(async () => {
    PolymerTest.clearBody();

    c2cInput = document.createElement('c2c-code-input');
    c2cInput.length = 6;
    document.body.appendChild(c2cInput);

    await waitAfterNextRender();
  });

  test('value set correctly', () => {
    const testValue = 'hello';
    c2cInput.setValue(testValue);
    assertEquals(c2cInput.value, testValue);
  });

  test('focus advances on input', () => {
    c2cInput.clearInput();
    c2cInput.focusInput();

    assertEquals(c2cInput.getFocusedIndex(), 0);

    c2cInput.getInput(0).value = 'a';
    c2cInput.getInput(0).dispatchEvent(new InputEvent('input'));
    assertEquals(c2cInput.getFocusedIndex(), 1);

    c2cInput.getInput(1).value = 'b';
    c2cInput.getInput(1).dispatchEvent(new InputEvent('input'));
    assertEquals(c2cInput.getFocusedIndex(), 2);

  });

  test('focus does not advance if it is the last box', () => {
    c2cInput.clearInput();

    c2cInput.getInput(5).focusInput();
    assertEquals(c2cInput.getFocusedIndex(), 5);
    c2cInput.getInput(5).dispatchEvent(new InputEvent('input'));
    assertEquals(c2cInput.getFocusedIndex(), 5);
  });

  test(
    'backspace on an empty input erases and focuses the previous input',
    () => {
      c2cInput.clearInput();
      const input1 = c2cInput.getInput(1);
      const input2 = c2cInput.getInput(2);
      input1.value = 'a';
      input2.focusInput();

      assertEquals(input1.value, 'a');
      input2.dispatchEvent(new KeyboardEvent('keydown', {'key': 'Backspace'}));
      assertEquals(c2cInput.getFocusedIndex(), 1);
      assertEquals(input1.value, '');
    }
  );

  test('backspace on filled input does not change focus', () => {
    c2cInput.clearInput();
    const input1 = c2cInput.getInput(1);
    const input2 = c2cInput.getInput(2);
    input1.value = 'a';
    input2.value = 'b';
    input2.focusInput();

    assertEquals(input1.value, 'a');
    input2.dispatchEvent(new KeyboardEvent('keydown', {'key': 'Backspace'}));
    assertEquals(c2cInput.getFocusedIndex(), 2);
    assertEquals(input1.value, 'a');
  });
});