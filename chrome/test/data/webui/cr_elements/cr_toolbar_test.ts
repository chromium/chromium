// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-toolbar. */

// clang-format off
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import type {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-toolbar', function() {
  let toolbar: CrToolbarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('AlwaysShowLogo', async function() {
    toolbar = document.createElement('cr-toolbar');
    document.body.appendChild(toolbar);

    toolbar.narrow = true;
    await toolbar.updateComplete;
    assertFalse(isVisible(toolbar.shadowRoot!.querySelector('picture')));

    toolbar.alwaysShowLogo = true;
    await toolbar.updateComplete;
    assertTrue(isVisible(toolbar.shadowRoot!.querySelector('picture')));
  });

  test('Custom logo', function() {
    document.body.innerHTML = getTrustedHtml(`
      <cr-toolbar id="toolbar">
        <div id="logo" slot="product-logo">New logo</div>
      </cr-toolbar>
    `);

    const toolbar = document.body.querySelector('cr-toolbar');
    assertTrue(!!toolbar);

    // There is no picture element because it's replaced with slot.
    assertFalse(isVisible(toolbar.shadowRoot!.querySelector('picture')));

    const newLogo = toolbar.querySelector('#logo');
    assertTrue(!!newLogo);
    assertEquals('product-logo', newLogo.assignedSlot!.name);
    assertEquals('New logo', newLogo.textContent!.trim());
  });

  test('Sends input aria-description', () => {
    document.body.innerHTML = getTrustedHtml(`
      <cr-toolbar id="toolbar" search-input-aria-description="my description">
      </cr-toolbar>
    `);

    const toolbar = document.body.querySelector('cr-toolbar');
    assertTrue(!!toolbar);
    assertEquals('my description', toolbar.getSearchField().inputAriaDescription);
  });
});
