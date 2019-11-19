// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension options dialog.
 * These are run as part of interactive_ui_tests.
 */

import 'chrome://extensions/extensions.js';

import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';

import {eventToPromise} from '../test_util.m.js';

suite('ExtensionOptionsDialogTest', () => {
  test('show options dialog', async () => {
    PolymerTest.clearBody();
    window.history.replaceState(
        {}, '', '/?id=ibbpngabdmdpednkhonkkobdeccpkiff');

    const manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);
    assertTrue(!!manager);
    await eventToPromise('view-enter-start', manager);
    const extensionDetailView = manager.$$('extensions-detail-view');
    assertTrue(!!extensionDetailView);

    const optionsButton = extensionDetailView.$$('#extensions-options');
    optionsButton.click();
    await eventToPromise('cr-dialog-open', manager);
    const dialog = manager.$$('#options-dialog');
    let waitForClose = eventToPromise('close', dialog);
    dialog.$.dialog.cancel();
    await waitForClose;
    const activeElement = getDeepActiveElement();
    assertEquals('CR-ICON-BUTTON', activeElement.tagName);
    assertEquals(optionsButton.$.icon, getDeepActiveElement());
  });
});
