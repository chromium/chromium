// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {SharePasswordFlowElement} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const SITE = 'test.com';

function startPasswordShare(passwordName: string = SITE):
    SharePasswordFlowElement {
  const shareElement = document.createElement('share-password-flow');
  shareElement.passwordName = passwordName;
  document.body.appendChild(shareElement);
  flush();
  return shareElement;
}

suite('SharePasswordFlowTest', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('Has correct loading state', async function() {
    const shareElement = startPasswordShare(/*passwordName=*/ SITE);
    await flushTasks();

    const dialog =
        shareElement.shadowRoot!.querySelector('share-password-loading-dialog');
    assertTrue(!!dialog);

    const header =
        dialog.shadowRoot!.querySelector('share-password-dialog-header');
    assertTrue(!!header);
    assertEquals(
        shareElement.i18n('shareDialogTitle', SITE), header.innerHTML!.trim());

    const spinner = dialog.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);
  });
});
