// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://access-code-cast/code_input/code_input.js';

suite('CodeInputElementTest', () => {
  /** @type {!CodeInputElement} */
  let c2cInput;

  /** @type {!CrInputElement} */
  let crInput;

  setup(() => {
    PolymerTest.clearBody();

    c2cInput = document.createElement('c2c-code-input');
    document.body.appendChild(c2cInput);
    crInput = c2cInput.crInput;
  });

  test('value set correctly', () => {
    c2cInput.value = 'hello';
    assertEquals(c2cInput.value, crInput.value);

    // |value| is copied to uppercase when typing triggers inputEvent.
    let testString = 'hello world';
    crInput.value = testString;
    crInput.dispatchEvent(new InputEvent('input'));
    assertEquals(c2cInput.value, testString.toUpperCase());
  });
});