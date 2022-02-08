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
    assertEquals(c2cInput.value, testValue.toUpperCase());
    assertEquals(c2cInput.$.inputElement.value, testValue);
    assertEquals(
      c2cInput.getDisplayChar(0).innerHTML,
      testValue[0].toUpperCase()
    );
    assertEquals(
      c2cInput.getDisplayChar(1).innerHTML,
      testValue[1].toUpperCase()
    );
    assertEquals(
      c2cInput.getDisplayChar(2).innerHTML,
      testValue[2].toUpperCase()
    );
    assertEquals(
      c2cInput.getDisplayChar(3).innerHTML,
      testValue[3].toUpperCase()
    );
    assertEquals(
      c2cInput.getDisplayChar(4).innerHTML,
      testValue[4].toUpperCase()
    );
    assertEquals(c2cInput.getDisplayChar(5).innerHTML, '');
  });

  test('focus is displayed properly', () => {
    c2cInput.clearInput();
    c2cInput.focusInput();

    assertTrue(c2cInput.getBox(0).classList.contains('focused'));
    assertTrue(c2cInput.getBox(1).classList.contains('focused'));
    assertTrue(c2cInput.getBox(2).classList.contains('focused'));
    assertTrue(c2cInput.getBox(3).classList.contains('focused'));
    assertTrue(c2cInput.getBox(4).classList.contains('focused'));
    assertTrue(c2cInput.getBox(5).classList.contains('focused'));
  });

  test('disabled state propogates correctly', () => {
    c2cInput.clearInput();
    c2cInput.disabled = false;
    assertFalse(c2cInput.$.inputElement.disabled);

    c2cInput.disabled = true;
    assertTrue(c2cInput.$.inputElement.disabled);
  });

  test('selection is displayed properly', async () => {
    c2cInput.setValue('qwerty');
    c2cInput.$.inputElement.setSelectionRange(1, 3);
    c2cInput.$.inputElement.dispatchEvent(new Event('select'));
    await waitAfterNextRender();

    assertFalse(c2cInput.getBox(0).classList.contains('selected'));
    assertTrue(c2cInput.getBox(1).classList.contains('selected'));
    assertTrue(c2cInput.getBox(2).classList.contains('selected'));
    assertFalse(c2cInput.getBox(3).classList.contains('selected'));
    assertFalse(c2cInput.getBox(4).classList.contains('selected'));
    assertFalse(c2cInput.getBox(5).classList.contains('selected'));

    c2cInput.$.inputElement.setSelectionRange(2, 5);
    c2cInput.$.inputElement.dispatchEvent(new Event('select'));
    await waitAfterNextRender();

    assertFalse(c2cInput.getBox(0).classList.contains('selected'));
    assertFalse(c2cInput.getBox(1).classList.contains('selected'));
    assertTrue(c2cInput.getBox(2).classList.contains('selected'));
    assertTrue(c2cInput.getBox(3).classList.contains('selected'));
    assertTrue(c2cInput.getBox(4).classList.contains('selected'));
    assertFalse(c2cInput.getBox(5).classList.contains('selected'));
  });

  test('cursor is displayed properly', async () => {
    c2cInput.setValue('qwerty');
    c2cInput.$.inputElement.setSelectionRange(1, 1);
    c2cInput.$.inputElement.dispatchEvent(new Event('select'));
    await waitAfterNextRender();

    assertFalse(c2cInput.getBox(0).classList.contains('cursor-start'));
    assertTrue(c2cInput.getBox(0).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(1).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(2).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(3).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(4).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(5).classList.contains('cursor'));

    c2cInput.$.inputElement.setSelectionRange(0, 0);
    c2cInput.$.inputElement.dispatchEvent(new Event('select'));
    await waitAfterNextRender();

    assertTrue(c2cInput.getBox(0).classList.contains('cursor-start'));
    assertFalse(c2cInput.getBox(0).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(1).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(2).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(3).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(4).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(5).classList.contains('cursor'));

    c2cInput.$.inputElement.setSelectionRange(4, 4);
    c2cInput.$.inputElement.dispatchEvent(new Event('select'));
    await waitAfterNextRender();

    assertFalse(c2cInput.getBox(0).classList.contains('cursor-start'));
    assertFalse(c2cInput.getBox(0).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(1).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(2).classList.contains('cursor'));
    assertTrue(c2cInput.getBox(3).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(4).classList.contains('cursor'));
    assertFalse(c2cInput.getBox(5).classList.contains('cursor'));
  });
});
