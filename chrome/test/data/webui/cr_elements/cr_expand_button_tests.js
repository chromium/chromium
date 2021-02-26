// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
// clang-format on

suite('cr-expand-button', function() {
  /** @type {!CrExpandButtonElement} */
  let button;

  /** @type {!CrIconButtonElement} */
  let icon;

  setup(() => {
    document.body.innerHTML = '';
    button = /** @type {!CrExpandButtonElement} */ (
        document.createElement('cr-expand-button'));
    document.body.appendChild(button);
    icon = /** @type {!CrIconButtonElement} */ (button.$$('#icon'));
  });

  test('setting |aria-label| label', () => {
    assertFalse(!!button.ariaLabel);
    assertEquals('label', icon.getAttribute('aria-labelledby'));
    assertEquals(null, icon.getAttribute('aria-label'));
    const ariaLabel = 'aria-label label';
    button.ariaLabel = ariaLabel;
    assertEquals(null, icon.getAttribute('aria-labelledby'));
    assertEquals(ariaLabel, icon.getAttribute('aria-label'));
  });

  test('changing |expanded|', () => {
    assertFalse(button.expanded);
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertEquals('cr:expand-more', icon.ironIcon);
    button.expanded = true;
    assertEquals('true', icon.getAttribute('aria-expanded'));
    assertEquals('cr:expand-less', icon.ironIcon);
  });

  test('changing |disabled|', () => {
    assertFalse(button.disabled);
    assertEquals('false', icon.getAttribute('aria-expanded'));
    assertFalse(icon.disabled);
    button.disabled = true;
    assertFalse(icon.hasAttribute('aria-expanded'));
    assertTrue(icon.disabled);
  });

  // Ensure that the label is marked with aria-hidden="true", so that screen
  // reader focus goes straight to the cr-icon-button.
  test('label aria-hidden', () => {
    const labelId = 'label';
    assertEquals('true', button.$$(`#${labelId}`).getAttribute('aria-hidden'));
    assertEquals(labelId, icon.getAttribute('aria-labelledby'));
  });
});
