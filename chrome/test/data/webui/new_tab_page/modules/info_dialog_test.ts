// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InfoDialogElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NewTabPageModulesInfoDialogTest', () => {
  let infoDialog: InfoDialogElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    infoDialog = new InfoDialogElement();
    document.body.appendChild(infoDialog);
  });

  test('can open dialog', () => {
    assertFalse(infoDialog.$.dialog.open);
    infoDialog.showModal();
    assertTrue(infoDialog.$.dialog.open);
  });

  test('clicking close button closes cr dialog', () => {
    // Arrange.
    infoDialog.showModal();

    // Act.
    infoDialog.$.closeButton.click();

    // Assert.
    assertFalse(infoDialog.$.dialog.open);
  });

  test('show-on-attach', () => {
    // Arrange.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    infoDialog = new InfoDialogElement();
    infoDialog.showOnAttach = true;

    // Act.
    document.body.appendChild(infoDialog);

    // Assert.
    assertTrue(infoDialog.$.dialog.open);
  });
});
