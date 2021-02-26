// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';

import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';
// clang-format on

suite('cr-expand-button-focus-tests', () => {
  /** @type {!CrExpandButtonElement} */
  let button;

  /** @type {!CrIconButtonElement} */
  let icon;

  /** @param {boolean} rippleShown */
  function assertRippleState(rippleShown) {
    assertEquals(icon, getDeepActiveElement());
    assertEquals(rippleShown, icon.getRipple().holdDown);
  }

  function assertRipple() {
    assertRippleState(true);
  }

  function assertNoRipple() {
    assertRippleState(false);
  }

  /**
   * @param {!Function} toggler
   * @return {!Promise<void>}
   */
  function waitForExpansion(toggler) {
    const wait = eventToPromise('expanded-changed', button);
    toggler();
    return wait;
  }

  /** @return {!Promise<void>} */
  function click() {
    return waitForExpansion(() => {
      // This is used in focusWithoutInk to change mode into ink hidden
      // when focused.
      button.dispatchEvent(new PointerEvent('pointerdown'));
      // Used to simulate releasing the mouse button.
      icon.fire('up');
      // When the mouse is pressed and released, it will also emit a 'click'
      // event which is used in cr-expand-button to toggle expansion.
      button.fire('click');
    });
  }

  /** @return {!Promise<void>} */
  function enter() {
    return waitForExpansion(() => {
      pressAndReleaseKeyOn(icon, 0, '', 'Enter');
    });
  }

  /** @return {!Promise<void>} */
  function space() {
    return waitForExpansion(() => {
      pressAndReleaseKeyOn(icon, 0, '', ' ');
    });
  }

  setup(() => {
    document.body.innerHTML = '';
    button = /** @type {!CrExpandButtonElement} */ (
        document.createElement('cr-expand-button'));
    document.body.appendChild(button);
    icon = /** @type {!CrIconButtonElement} */ (button.$$('#icon'));
  });

  test('focus, ripple', () => {
    button.focus();
    assertRipple();
  });

  test('click, no ripple', async () => {
    await click();
    assertNoRipple();
  });

  test('enter, ripple', async () => {
    await enter();
    assertRipple();
  });

  test('space, ripple', async () => {
    await space();
    assertRipple();
  });

  test('focus then click, no ripple', async () => {
    button.focus();
    await click();
    assertNoRipple();
  });

  test('click then enter, no ripple', async () => {
    await click();
    await enter();
    assertNoRipple();
  });

  test('click then space, no ripple', async () => {
    await click();
    await space();
    assertNoRipple();
  });

  test('enter then click, no ripple', async () => {
    await enter();
    await click();
    assertNoRipple();
  });

  test('space then click, no ripple', async () => {
    await space();
    await click();
    assertNoRipple();
  });

  test('focus then enter, ripple', async () => {
    button.focus();
    await enter();
    assertRipple();
  });

  test('focus then space, ripple', async () => {
    button.focus();
    await space();
    assertRipple();
  });
});
