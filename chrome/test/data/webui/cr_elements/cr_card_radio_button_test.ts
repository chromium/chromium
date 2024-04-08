// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_radio_button/cr_card_radio_button.js';

import type {CrCardRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_card_radio_button.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// clang-format on

suite('cr-card-radio-button', function() {
  let radioButton: CrCardRadioButtonElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    radioButton = document.createElement('cr-card-radio-button');
    document.body.appendChild(radioButton);
  });

  function assertChecked() {
    assertTrue(radioButton.hasAttribute('checked'));
    assertEquals('true', radioButton.$.button.getAttribute('aria-checked'));
    assertTrue(
        getComputedStyle(radioButton.shadowRoot!.querySelector('#checkMark')!)
            .display !== 'none');
  }

  function assertNotChecked() {
    assertFalse(radioButton.hasAttribute('checked'));
    assertEquals('false', radioButton.$.button.getAttribute('aria-checked'));
    assertTrue(
        getComputedStyle(radioButton.shadowRoot!.querySelector('#checkMark')!)
            .display === 'none');
  }

  function assertDisabled() {
    assertTrue(radioButton.hasAttribute('disabled'));
    assertEquals('true', radioButton.$.button.getAttribute('aria-disabled'));
    assertEquals('none', getComputedStyle(radioButton).pointerEvents);
    assertEquals('1', getComputedStyle(radioButton).opacity);
  }

  function assertNotDisabled() {
    assertFalse(radioButton.hasAttribute('disabled'));
    assertEquals('false', radioButton.$.button.getAttribute('aria-disabled'));
    assertEquals('1', getComputedStyle(radioButton).opacity);
  }

  // Setting selection by mouse/keyboard is cr-radio-group's job, so
  // these tests simply set states programmatically and make sure the element
  // is visually correct.
  test('Checked', async () => {
    assertNotChecked();
    radioButton.checked = true;
    await radioButton.updateComplete;
    assertChecked();
    radioButton.checked = false;
    await radioButton.updateComplete;
    assertNotChecked();
  });

  test('Disabled', async () => {
    assertNotDisabled();
    radioButton.disabled = true;
    await radioButton.updateComplete;
    assertDisabled();
    radioButton.disabled = false;
    await radioButton.updateComplete;
    assertNotChecked();
  });

  test('Ripple', function() {
    function getRipple() {
      return radioButton.shadowRoot!.querySelector('cr-ripple');
    }

    assertFalse(!!getRipple());
    radioButton.dispatchEvent(
        new CustomEvent('up', {bubbles: true, composed: true}));
    const ripple = getRipple();
    assertTrue(!!ripple);
    assertFalse(ripple.holdDown);
  });
});
