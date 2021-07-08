// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/action_toolbar.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';

export function actionToolbarTest() {
  /** @type {?ActionToolbarElement} */
  let actionToolbar = null;

  setup(() => {
    actionToolbar = /** @type {!ActionToolbarElement} */ (
        document.createElement('action-toolbar'));
    assertTrue(!!actionToolbar);
    document.body.appendChild(actionToolbar);
  });

  teardown(() => {
    if (actionToolbar) {
      actionToolbar.remove();
    }
    actionToolbar = null;
  });

  // Verify the page count text updates when the number of scanned images
  // changes.
  test('totalPageCountIncrements', () => {
    assertEquals('', actionToolbar.$$('#pageNumbers').textContent.trim());

    actionToolbar.numTotalPages = 3;
    assertEquals('', actionToolbar.$$('#pageNumbers').textContent.trim());

    actionToolbar.numTotalPages = 3;
    actionToolbar.currentPageInView = 1;
    assertEquals('1 of 3', actionToolbar.$$('#pageNumbers').textContent.trim());

    actionToolbar.numTotalPages = 4;
    actionToolbar.currentPageInView = 2;
    assertEquals('2 of 4', actionToolbar.$$('#pageNumbers').textContent.trim());
  });
}
