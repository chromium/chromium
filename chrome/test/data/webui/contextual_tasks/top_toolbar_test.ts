// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/top_toolbar.js';

import type {TopToolbarElement} from 'chrome://contextual-tasks/top_toolbar.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('TopToolbarTest', () => {
  let topToolbar: TopToolbarElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    topToolbar = document.createElement('top-toolbar');
    document.body.appendChild(topToolbar);
  });


  test('shows correct logo', () => {
    assertEquals(
        topToolbar.$.topToolbarLogo.src,
        // <if expr="_google_chrome">
        'chrome://resources/cr_components/searchbox/icons/google_g_gradient.svg',
        // </if>
        // <if expr="not _google_chrome">
        'chrome://resources/cr_components/searchbox/icons/chrome_product.svg',
        // </if>
    );
  });
});
