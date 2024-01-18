// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-toolbar. */

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar.js';

import {CrToolbarElement} from 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

suite('cr-toolbar', function() {
  let toolbar: CrToolbarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbar = document.createElement('cr-toolbar');
    document.body.appendChild(toolbar);
  });

  test('autofocus propagated to search field', () => {
    assertFalse(toolbar.autofocus);
    assertFalse(toolbar.getSearchField().hasAttribute('autofocus'));

    toolbar.autofocus = true;
    assertTrue(toolbar.getSearchField().hasAttribute('autofocus'));
  });

  test('FocusesMenuButton', async () => {
    toolbar.showMenu = true;
    toolbar.focusMenuButton();
    await new Promise(resolve => requestAnimationFrame(resolve));
    assertEquals(
        toolbar.shadowRoot!.querySelector('#menuButton'),
        toolbar.shadowRoot!.activeElement);
  });

  test('ReturnsIfMenuIsFocused', async () => {
    assertFalse(toolbar.isMenuFocused());
    toolbar.showMenu = true;
    await new Promise(resolve => requestAnimationFrame(resolve));
    toolbar.shadowRoot!.querySelector<HTMLElement>('#menuButton')!.focus();
    assertTrue(toolbar.isMenuFocused());
  });
});
