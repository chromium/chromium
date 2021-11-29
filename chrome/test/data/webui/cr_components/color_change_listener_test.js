// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {COLORS_CSS_SELECTOR, refreshColorCss} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';

suite('ColorChangeListenerTest', () => {
  setup(() => {
    document.body.innerHTML =
        '<link rel="stylesheet" href="chrome://theme/colors.css"/>';
  });

  test('CorrectlyUpdatesColorsStylesheetURL', () => {
    const colorCssNode = document.querySelector(COLORS_CSS_SELECTOR);
    assertTrue(!!colorCssNode);
    const initialHref = colorCssNode.getAttribute('href');
    assertTrue(initialHref.startsWith('chrome://theme/colors.css'));

    // refreshColorCss() should append search params to the colors CSS href.
    assertTrue(refreshColorCss());

    const finalHref = colorCssNode.getAttribute('href');
    assertTrue(finalHref.startsWith('chrome://theme/colors.css'));
    assertTrue(!!new URL(finalHref).search);

    assertNotEquals(initialHref, finalHref);
  });

  test('HandlesCasesWhereColorsStylesheetIsNotSetCorrectly', () => {
    // Handles the case where the link element exists but the attribute is
    // malformed.
    document.body.innerHTML =
        '<link rel="stylesheet" bad_href="chrome://theme/colors.css"/>';
    assertFalse(refreshColorCss());

    // Handles the case where the link element does not exist.
    document.body.innerHTML = '';
    assertFalse(refreshColorCss());
  });
});
