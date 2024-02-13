// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for managed-dialog. */

import 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';

import type {ManagedDialogElement} from 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ManagedDialogTest', function() {
  suiteSetup(function() {
    loadTimeData.data = {};
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /**
   * Sets loadTimeData to the parameters, inserts a <managed-dialog> element in
   * the DOM, and returns it.
   * @param title Managed dialog title text
   * @param body Managed dialog body text
   */
  function createManagedDialog(
      title: string, body: string): ManagedDialogElement {
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
    assertTrue(dialog.$.dialog.open);
    assertTrue(dialog.shadowRoot!.textContent!.includes(title));
    assertTrue(dialog.shadowRoot!.textContent!.includes(body));

    // Click OK button to dismiss dialog
    dialog.shadowRoot!.querySelector<HTMLElement>('.action-button')!.click();

    // Dialog is no longer displayed
    assertFalse(dialog.$.dialog.open);
  });
});
