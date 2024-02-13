// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-toolbar. */

// clang-format off
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-toolbar', function() {
  let toolbar: CrToolbarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbar = document.createElement('cr-toolbar');
    document.body.appendChild(toolbar);
  });

  test('autofocus propagated to search field', async () => {
    assertFalse(toolbar.autofocus);
    assertFalse(toolbar.getSearchField().hasAttribute('autofocus'));

    toolbar.autofocus = true;
    await toolbar.updateComplete;
    assertTrue(toolbar.getSearchField().hasAttribute('autofocus'));
  });

  test('FocusesMenuButton', async () => {
    toolbar.showMenu = true;
    await toolbar.updateComplete;
    toolbar.focusMenuButton();
    const menuButton = toolbar.shadowRoot!.querySelector('#menuButton');
    assertTrue(!!menuButton);
    await eventToPromise('focus', menuButton);
    assertEquals(menuButton, toolbar.shadowRoot!.activeElement);
    assertTrue(toolbar.isMenuFocused());
  });
});
