// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-toolbar. */

// clang-format off
import 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar.js';

import {CrToolbarElement} from 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-toolbar', function() {
  let toolbar: CrToolbarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('AlwaysShowLogo', function() {
    toolbar = document.createElement('cr-toolbar');
    document.body.appendChild(toolbar);

    toolbar.narrow = true;
    assertFalse(isVisible(toolbar.shadowRoot!.querySelector('picture')));

    toolbar.alwaysShowLogo = true;
    assertTrue(isVisible(toolbar.shadowRoot!.querySelector('picture')));
  });

  test('Custom logo', function() {
    document.body.innerHTML = getTrustedHtml(`
      <cr-toolbar id="toolbar">
        <div id="logo" slot="product-logo">New logo</div>
      </cr-toolbar>
    `);
    flush();

    const toolbar = document.body.querySelector('cr-toolbar');
    assertTrue(!!toolbar);

    // There is no picture element because it's replaced with slot.
    assertFalse(isVisible(toolbar.shadowRoot!.querySelector('picture')));

    const newLogo = toolbar.querySelector('#logo');
    assertTrue(!!newLogo);
    assertEquals('product-logo', newLogo.assignedSlot!.name);
    assertEquals('New logo', newLogo.textContent!.trim());
  });
});
