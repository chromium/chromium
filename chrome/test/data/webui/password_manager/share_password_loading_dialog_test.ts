// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {SharePasswordLoadingDialogElement} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const TITLE = 'dialog title';

suite('SharePasswordLoadingDialogTest', function() {
  let dialog: SharePasswordLoadingDialogElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('share-password-loading-dialog');
    dialog.dialogTitle = TITLE;
    document.body.appendChild(dialog);
    return flushTasks();
  });

  test('Has correct initial state', async function() {
    const header =
        dialog.shadowRoot!.querySelector('share-password-dialog-header');
    assertTrue(!!header);
    assertEquals(TITLE, header.innerHTML!.trim());

    const spinner = dialog.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);
  });
});
