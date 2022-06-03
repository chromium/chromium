// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_radio_button/cr_card_radio_button.m.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';

// clang-format on

suite('cr-card-radio-button', function() {
  /** @type {!CrCardRadioButtonElement} */
  let radioButton;

  setup(function() {
    document.body.innerHTML = '';
    radioButton = /** @type {!CrCardRadioButtonElement} */ (
        document.createElement('cr-card-radio-button'));
    document.body.appendChild(radioButton);
  });

  function assertChecked() {
    assertTrue(radioButton.hasAttribute('checked'));
    assertEquals('true', radioButton.$.button.getAttribute('aria-checked'));
    assertTrue(
        getComputedStyle(radioButton.$$('#checkMark')).display !== 'none');
  }

  function assertNotChecked() {
    assertFalse(radioButton.hasAttribute('checked'));
    assertEquals('false', radioButton.$.button.getAttribute('aria-checked'));
    assertTrue(
        getComputedStyle(radioButton.$$('#checkMark')).display === 'none');
  }

  function assertDisabled() {
    assertTrue(radioButton.hasAttribute('disabled'));
    assertEquals('true', radioButton.$.button.getAttribute('aria-disabled'));
    assertEquals('none', getComputedStyle(radioButton).pointerEvents);
    assertNotEquals('1', getComputedStyle(radioButton).opacity);
  }

  function assertNotDisabled() {
    assertFalse(radioButton.hasAttribute('disabled'));
    assertEquals('false', radioButton.$.button.getAttribute('aria-disabled'));
    assertEquals('1', getComputedStyle(radioButton).opacity);
  }

  // Setting selection by mouse/keyboard is cr-radio-group's job, so
  // these tests simply set states programmatically and make sure the element
  // is visually correct.
  test('Checked', () => {
    assertNotChecked();
    radioButton.checked = true;
    assertChecked();
    radioButton.checked = false;
    assertNotChecked();
  });

  test('Disabled', () => {
    assertNotDisabled();
    radioButton.disabled = true;
    assertDisabled();
    radioButton.disabled = false;
    assertNotChecked();
  });
});
