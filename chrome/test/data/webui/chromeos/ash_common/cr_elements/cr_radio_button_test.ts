// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';

import {CrRadioButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import {assertEquals, assertNotEquals, assertFalse, assertTrue, assertLT, assertGT} from 'chrome://webui-test/chai_assert.js';
// clang-format on

suite('cr-radio-button', function() {
  let radioButton: CrRadioButtonElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    radioButton = document.createElement('cr-radio-button');
    document.body.appendChild(radioButton);
  });

  function assertStyle(element: Element, name: string, expected: string) {
    const actual = getComputedStyle(element).getPropertyValue(name).trim();
    assertEquals(expected, actual);
  }

  function assertNotStyle(element: Element, name: string, not: string) {
    const actual = getComputedStyle(element).getPropertyValue(name).trim();
    assertNotEquals(not, actual);
  }

  function assertChecked() {
    assertTrue(radioButton.hasAttribute('checked'));
    assertEquals('true', radioButton.$.button.getAttribute('aria-checked'));
    assertNotStyle(
        radioButton.shadowRoot!.querySelector('.disc')!, 'background-color',
        'rgba(0, 0, 0, 0)');
  }

  function assertNotChecked() {
    assertFalse(radioButton.hasAttribute('checked'));
    assertEquals('false', radioButton.$.button.getAttribute('aria-checked'));
    assertStyle(
        radioButton.shadowRoot!.querySelector('.disc')!, 'background-color',
        'rgba(0, 0, 0, 0)');
    assertStyle(
        radioButton.shadowRoot!.querySelector('.disc')!, 'background-color',
        'rgba(0, 0, 0, 0)');
  }

  function assertDisabled() {
    assertTrue(radioButton.hasAttribute('disabled'));
    assertEquals('true', radioButton.$.button.getAttribute('aria-disabled'));
    assertStyle(radioButton, 'pointer-events', 'none');
    assertNotStyle(radioButton, 'opacity', '1');
  }

  function assertNotDisabled() {
    assertFalse(radioButton.hasAttribute('disabled'));
    assertEquals('false', radioButton.$.button.getAttribute('aria-disabled'));
    assertStyle(radioButton, 'opacity', '1');
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
    assertFalse(!!radioButton.shadowRoot!.querySelector('paper-ripple'));
    radioButton.dispatchEvent(
        new CustomEvent('focus', {bubbles: true, composed: true}));
    assertTrue(!!radioButton.shadowRoot!.querySelector('paper-ripple'));
    assertTrue(radioButton.shadowRoot!.querySelector('paper-ripple')!.holdDown);
    radioButton.dispatchEvent(
        new CustomEvent('up', {bubbles: true, composed: true}));
    assertFalse(
        radioButton.shadowRoot!.querySelector('paper-ripple')!.holdDown);
  });

  test('Label Hidden', function() {
    // Having no label set hides label.
    assertStyle(
        radioButton.shadowRoot!.querySelector('#label')!, 'display', 'none');

    // Setting label shows label.
    radioButton.label = 'foo';
    assertNotStyle(
        radioButton.shadowRoot!.querySelector('#label')!, 'display', 'none');
    assertNotStyle(
        radioButton.shadowRoot!.querySelector('#label')!, 'clip',
        'rect(0px, 0px, 0px, 0px)');
    assertEquals(radioButton.$.button.getAttribute('aria-labelledby'), 'label');
    assertEquals(
        radioButton.shadowRoot!.querySelector('#label')!.textContent!.trim(),
        'foo');

    // Setting hideLabelText true clips label from screen reader.
    radioButton.hideLabelText = true;
    assertStyle(
        radioButton.shadowRoot!.querySelector('#label')!, 'clip',
        'rect(0px, 0px, 0px, 0px)');
    assertEquals(radioButton.$.button.getAttribute('aria-labelledby'), 'label');
    assertEquals(
        radioButton.shadowRoot!.querySelector('#label')!.textContent!.trim(),
        'foo');
  });

  test('Label First', () => {
    const button = radioButton.$.button;
    let buttonRect = button.getBoundingClientRect();
    const labelWrapper = radioButton.shadowRoot!.querySelector('#labelWrapper');
    assertTrue(!!labelWrapper);

    let labelWrapperRect = labelWrapper.getBoundingClientRect();
    assertLT(buttonRect.left, labelWrapperRect.left);

    radioButton.classList.add('label-first');
    buttonRect = button.getBoundingClientRect();
    labelWrapperRect = labelWrapper.getBoundingClientRect();
    assertGT(buttonRect.left, labelWrapperRect.left);
  });
});
