// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
//
// #import {eventToPromise} from '../test_util.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
// clang-format on

suite('cr-expand-button-focus-tests', () => {
  let button;

  /** @param {boolean} */
  function assertRippleState(rippleShown) {
    assertEquals(button.$.icon, getDeepActiveElement());
    assertEquals(rippleShown, button.$.icon.getRipple().holdDown);
  }

  function assertRipple() {
    assertRippleState(true);
  }

  function assertNoRipple() {
    assertRippleState(false);
  }

  /** @param {!Function} toggler */
  async function waitForExpansion(toggler) {
    const wait = test_util.eventToPromise('expanded-changed', button);
    toggler();
    await wait;
  }

  async function click() {
    await waitForExpansion(() => {
      // This is used in cr.ui.focusWithoutInk to change mode into ink hidden
      // when focused.
      button.dispatchEvent(new PointerEvent('pointerdown'));
      // Used to simulate releasing the mouse button.
      button.$.icon.fire('up');
      // When the mouse is pressed and released, it will also emit a 'click'
      // event which is used in cr-expand-button to toggle expansion.
      button.fire('click');
    });
  }

  async function enter() {
    await waitForExpansion(() => {
      MockInteractions.pressAndReleaseKeyOn(button.$.icon, '', '', 'Enter');
    });
  }

  async function space() {
    await waitForExpansion(() => {
      MockInteractions.pressAndReleaseKeyOn(button.$.icon, '', '', ' ');
    });
  }

  setup(() => {
    document.body.innerHTML = '<cr-expand-button></cr-expand-button>';
    button = document.body.querySelector('cr-expand-button');
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
