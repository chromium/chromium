// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://internet-config-dialog/internet_config_dialog.js';

import {InternetConfigDialogElement} from 'chrome://internet-config-dialog/internet_config_dialog.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('internet-config-dialog', () => {
  let internetConfigDialog: InternetConfigDialogElement;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  async function init() {
    internetConfigDialog = document.createElement('internet-config-dialog');
    document.body.appendChild(internetConfigDialog);
    await flushAsync();
  }

  test('Out of range not shown when first open the dialog', async () => {
    await init();
    internetConfigDialog.setErrorForTesting('bad-passphrase');
    await flushAsync();

    assertTrue(!!internetConfigDialog.shadowRoot!.querySelector<HTMLDivElement>(
        '#errorDiv'));

    internetConfigDialog.setErrorForTesting('out-of-range');
    await flushAsync();
    assertFalse(
        !!internetConfigDialog.shadowRoot!.querySelector<HTMLDivElement>(
            '#errorDiv'));
  });
});
