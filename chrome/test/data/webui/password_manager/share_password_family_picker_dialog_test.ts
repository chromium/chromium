// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {SyncBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {makeRecipientInfo} from './test_util.js';


function assertVisibleTextContent(element: HTMLElement, expectedText: string) {
  assertTrue(isVisible(element));
  assertEquals(expectedText, element?.textContent!.trim());
}

suite('SharePasswordFamilyPickerDialogTest', function() {
  let syncProxy: TestSyncBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
    return flushTasks();
  });

  test('Has correct initial state', async function() {
    const expectedTitle = 'test.com';
    syncProxy.accountInfo = {
      email: 'test@gmail.com',
      avatarImage: 'chrome://image-url/',
    };

    const dialog =
        document.createElement('share-password-family-picker-dialog');
    dialog.dialogTitle = expectedTitle;
    dialog.members = [makeRecipientInfo()];
    document.body.appendChild(dialog);
    await flushTasks();

    dialog.querySelectorAll('share-password-recipient').forEach(element => {
      assertTrue(isVisible(element));
    });

    assertVisibleTextContent(dialog.$.header, expectedTitle);
    assertVisibleTextContent(
        dialog.$.description,
        dialog.i18n('sharePasswordFamilyPickerDescription'));
    assertVisibleTextContent(dialog.$.cancel, dialog.i18n('cancel'));
    assertVisibleTextContent(dialog.$.action, dialog.i18n('share'));

    assertEquals(syncProxy.accountInfo.avatarImage, dialog.$.avatar.src);
    assertEquals(dialog.$.manageLink.href, dialog.i18n('familyGroupSiteURL'));
    assertVisibleTextContent(
        dialog.$.footerDescription,
        dialog.i18n('sharePasswordManageFamily') + ' • ' +
            syncProxy.accountInfo.email);
  });

  test('Cancel should notify parent to close the dialog', async function() {
    const dialog =
        document.createElement('share-password-family-picker-dialog');
    document.body.appendChild(dialog);
    await flushTasks();

    const closeDialog = eventToPromise('close', dialog);
    dialog.$.cancel.click();
    await closeDialog;
  });
});
