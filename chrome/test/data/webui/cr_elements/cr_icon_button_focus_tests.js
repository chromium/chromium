// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
//
// #import {flushTasks} from '../test_util.m.js';
// clang-format on

suite('cr-icon-button-focus-tests', function() {
  let button;

  setup(async () => {
    PolymerTest.clearBody();
    button = document.createElement('cr-icon-button');
    document.body.appendChild(button);
    await test_util.flushTasks();
  });

  test('focus shows ripple', () => {
    button.focus();
    assertTrue(button.getRipple().holdDown);
    button.blur();
    assertFalse(button.getRipple().holdDown);
  });

  test('when disabled, focus does not show ripple', () => {
    button.disabled = true;
    button.focus();
    assertFalse(button.getRipple().holdDown);
    button.blur();
    button.disabled = false;
    button.focus();
    assertTrue(button.getRipple().holdDown);
    // Settings |disabled| to true does remove an existing ripple.
    button.disabled = true;
    assertFalse(button.getRipple().holdDown);
  });

  test('when noink, focus does not show ripple', () => {
    button.noink = true;
    button.focus();
    assertFalse(button.getRipple().holdDown);
    button.blur();
    button.noink = false;
    button.focus();
    assertTrue(button.getRipple().holdDown);
    // Setting |noink| to true does not remove an existing ripple.
    button.noink = true;
    assertTrue(button.getRipple().holdDown);
  });

  test('no ripple until focus', () => {
    assertFalse(button.hasRipple());
    button.focus();
    assertTrue(button.hasRipple());
  });

  test('when noink, no ripple until mouse down', () => {
    button.noink = true;
    button.focus();
    assertFalse(button.hasRipple());
    button.dispatchEvent(new PointerEvent('pointerdown'));
    assertTrue(button.hasRipple());
  });
});
