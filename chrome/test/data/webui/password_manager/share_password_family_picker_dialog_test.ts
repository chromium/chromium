// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {SyncBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {makeRecipientInfo} from './test_util.js';


function assertVisibleTextContent(element: HTMLElement, expectedText: string) {
  assertTrue(isVisible(element));
  assertEquals(expectedText, element?.textContent!.trim());
}

function countSelectedRecipients(dialog: HTMLElement): number {
  return Array
      .from(dialog.shadowRoot!.querySelectorAll('share-password-recipient'))
      .filter(item => item.selected)
      .length;
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

    dialog.shadowRoot!.querySelectorAll('share-password-recipient')
        .forEach(element => {
          assertTrue(isVisible(element));
        });

    assertVisibleTextContent(dialog.$.header, expectedTitle);
    assertVisibleTextContent(
        dialog.$.description,
        dialog.i18n('sharePasswordFamilyPickerDescription'));
    assertVisibleTextContent(dialog.$.cancel, dialog.i18n('cancel'));
    assertVisibleTextContent(dialog.$.action, dialog.i18n('share'));

    assertEquals(syncProxy.accountInfo.avatarImage, dialog.$.avatar.src);
    assertEquals(dialog.$.viewFamily.href, dialog.i18n('familyGroupViewURL'));
    assertVisibleTextContent(
        dialog.$.footerDescription,
        dialog.i18n('sharePasswordViewFamily') + ' • ' +
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

  test('Share button is available when a member is selected', async function() {
    const dialog =
        document.createElement('share-password-family-picker-dialog');
    dialog.members =
        [makeRecipientInfo(), makeRecipientInfo(/*isEligible=*/ false)];
    document.body.appendChild(dialog);
    await flushTasks();

    assertTrue(dialog.$.action.disabled);

    dialog.shadowRoot!.querySelectorAll('share-password-recipient')
        .forEach(element => {
          assertFalse(element.selected);
          element.click();
        });
    await flushTasks();

    assertFalse(dialog.$.action.disabled);
    assertEquals(1, dialog.selectedRecipients.length);
  });

  test(
      'Single family member is not pre-selected if ineligible',
      async function() {
        const dialog =
            document.createElement('share-password-family-picker-dialog');
        dialog.members = [makeRecipientInfo(/*isEligible=*/ false)];
        document.body.appendChild(dialog);
        await flushTasks();

        assertEquals(countSelectedRecipients(dialog), 0);
        assertTrue(dialog.$.action.disabled);
      });

  test('Single family member is pre-selected if eligible', async function() {
    const dialog =
        document.createElement('share-password-family-picker-dialog');
    dialog.members = [makeRecipientInfo(/*isEligible=*/ true)];
    document.body.appendChild(dialog);
    await flushTasks();

    assertEquals(countSelectedRecipients(dialog), 1);
    assertFalse(dialog.$.action.disabled);
  });

  test('Multiple eligble members are not pre-selected', async function() {
    const dialog =
        document.createElement('share-password-family-picker-dialog');
    dialog.members = [makeRecipientInfo(), makeRecipientInfo()];
    document.body.appendChild(dialog);
    await flushTasks();

    assertEquals(countSelectedRecipients(dialog), 0);
    assertTrue(dialog.$.action.disabled);
  });

  test(
      'Single eligible member not pre-selected if ineligible members present',
      async function() {
        const dialog =
            document.createElement('share-password-family-picker-dialog');
        dialog.members = [makeRecipientInfo(), makeRecipientInfo()];
        document.body.appendChild(dialog);
        await flushTasks();

        assertEquals(countSelectedRecipients(dialog), 0);
        assertTrue(dialog.$.action.disabled);
      });


  test('Action button dispatches start-share event', async function() {
    const dialog =
        document.createElement('share-password-family-picker-dialog');

    dialog.members = [makeRecipientInfo(), makeRecipientInfo()];
    document.body.appendChild(dialog);
    await flushTasks();

    dialog.shadowRoot!.querySelectorAll('share-password-recipient')
        .forEach(element => {
          element.click();
        });
    await flushTasks();

    const startShare = eventToPromise('start-share', dialog);
    dialog.$.action.click();
    await startShare;
  });
});
