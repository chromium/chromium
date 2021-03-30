// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for managed-dialog. */

import 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';

suite('ManagedDialogTest', function() {
  suiteSetup(function() {
    loadTimeData.data = {};
  });

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
    loadTimeData.overrideValues({
      title,
      body,
      controlledSettingPolicy: '',
      ok: 'OK',
      close: 'Close',
    });

    const dialog = document.createElement('managed-dialog');
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

    // Click OK button to dismiss dialog
    dialog.$$('.action-button').click();

    // Dialog is no longer displayed
    assertFalse(dialog.$$('cr-dialog').open);
  });
});
