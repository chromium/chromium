// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for managed-dialog. */

import 'chrome://settings/lazy_load.js';

import {assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';

suite('SettingsManagedDialog', function() {
  setup(function() {
    document.body.innerHTML = '';
  });

  /**
   * Sets loadTimeData to the parameters, inserts a <managed-dialog> element in
   * the DOM, and returns it.
   * @param {string} title Managed dialog title text
   * @param {string} body Managed dialog body text
   * @return {Element}
   */
  function createManagedDialog(title, body) {
    const dialog = document.createElement('settings-managed-dialog');
    dialog.title = title;
    dialog.body = body;
    document.body.appendChild(dialog);
    return dialog;
  }

  test('DialogTextContents', function() {
    const title = 'Roses are red, violets are blue.';
    const body = 'Your org is managing this for you!';
    const dialog = createManagedDialog(title, body);
    assertNotEquals('none', getComputedStyle(dialog).display);
    assertTrue(dialog.$$('cr-dialog').open);
    assertTrue(dialog.shadowRoot.textContent.includes(title));
    assertTrue(dialog.shadowRoot.textContent.includes(body));
  });

  test('DialogDismiss', function() {
    const dialog = createManagedDialog('', '');
    assertNotEquals('none', getComputedStyle(dialog).display);
    assertTrue(dialog.$$('cr-dialog').open);

    // Click OK button to dismiss dialog
    dialog.$$('.action-button').click();

    // Dialog is no longer displayed
    assertFalse(dialog.$$('cr-dialog').open);
  });
});
