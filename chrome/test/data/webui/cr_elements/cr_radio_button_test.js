// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
// clang-format on

suite('cr-radio-button', function() {
  /** @type {!CrRadioButtonElement} */
  let radioButton;

  setup(function() {
    document.body.innerHTML = '';
    radioButton = /** @type {!CrRadioButtonElement} */ (
        document.createElement('cr-radio-button'));
    document.body.appendChild(radioButton);
  });

  function assertChecked() {
    assertTrue(radioButton.hasAttribute('checked'));
    assertEquals('true', radioButton.$.button.getAttribute('aria-checked'));
    assertTrue(
        getComputedStyle(radioButton.$$('.disc')).backgroundColor !==
        'rgba(0, 0, 0, 0)');
  }

  function assertNotChecked() {
    assertFalse(radioButton.hasAttribute('checked'));
    assertEquals('false', radioButton.$.button.getAttribute('aria-checked'));
    assertEquals(
        'rgba(0, 0, 0, 0)',
        getComputedStyle(radioButton.$$('.disc')).backgroundColor);
  }

  function assertDisabled() {
    assertTrue(radioButton.hasAttribute('disabled'));
    assertEquals('true', radioButton.$.button.getAttribute('aria-disabled'));
    assertEquals('none', getComputedStyle(radioButton).pointerEvents);
    assertTrue('1' !== getComputedStyle(radioButton).opacity);
  }

  function assertNotDisabled() {
    assertFalse(radioButton.hasAttribute('disabled'));
    assertEquals('false', radioButton.$.button.getAttribute('aria-disabled'));
    assertEquals('1', getComputedStyle(radioButton).opacity);
  }

  // Setting selection by mouse/keyboard is cr-radio-group's job, so
  // these tests simply set states programatically and make sure the element
  // is visually correct.
  test('Checked', function() {
    assertNotChecked();
    radioButton.checked = true;
    assertChecked();
    radioButton.checked = false;
    assertNotChecked();
  });

  test('Disabled', function() {
    assertNotDisabled();
    radioButton.disabled = true;
    assertDisabled();
    radioButton.disabled = false;
    assertNotChecked();
  });

  test('Ripple', function() {
    assertFalse(!!radioButton.$$('paper-ripple'));
    radioButton.fire('focus');
    assertTrue(!!radioButton.$$('paper-ripple'));
    assertTrue(radioButton.$$('paper-ripple').holdDown);
    radioButton.fire('up');
    assertFalse(radioButton.$$('paper-ripple').holdDown);
  });
});
