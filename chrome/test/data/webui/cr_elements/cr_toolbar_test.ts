// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-toolbar. */

// clang-format off
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';

import {CrToolbarElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

suite('cr-toolbar', function() {
  let toolbar: CrToolbarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbar = document.createElement('cr-toolbar');
    document.body.appendChild(toolbar);
  });

  test('AlwaysShowLogo', function() {
    toolbar.narrow = true;
    assertFalse(isVisible(toolbar.shadowRoot!.querySelector('picture')));

    toolbar.alwaysShowLogo = true;
    assertTrue(isVisible(toolbar.shadowRoot!.querySelector('picture')));
  });
});
