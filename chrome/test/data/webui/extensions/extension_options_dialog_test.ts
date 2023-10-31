// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension options dialog.
 * These are run as part of interactive_ui_tests.
 */

import 'chrome://extensions/extensions.js';

import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('ExtensionOptionsDialogTest', () => {
  test('show options dialog', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState(
        {}, '', '/?id=ibbpngabdmdpednkhonkkobdeccpkiff');

    const manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);
    assertTrue(!!manager);
    await eventToPromise('view-enter-start', manager);
    const extensionDetailView =
        manager.shadowRoot!.querySelector('extensions-detail-view');
    assertTrue(!!extensionDetailView);

    const optionsButton = extensionDetailView.$.extensionsOptions;
    assertTrue(!!optionsButton);
    optionsButton.click();
    await eventToPromise('cr-dialog-open', manager);
    const dialog =
        manager.shadowRoot!.querySelector('extensions-options-dialog');
    assertTrue(!!dialog);
    const waitForClose = eventToPromise('close', dialog);
    dialog.$.dialog.cancel();
    await waitForClose;
    const activeElement = getDeepActiveElement();
    assertEquals('CR-ICON-BUTTON', activeElement!.tagName);
    assertEquals(
        optionsButton.shadowRoot!.querySelector('#icon'),
        getDeepActiveElement());
  });
});
