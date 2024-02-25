// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {SharePasswordFlowElement} from 'chrome://password-manager/password_manager.js';
import {PasswordManagerImpl, ShareFlowState} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {makeFamilyFetchResults, makeRecipientInfo} from './test_util.js';

const SITE = 'test.com';

function startPasswordShare(passwordName: string = SITE):
    SharePasswordFlowElement {
  const shareElement = document.createElement('share-password-flow');
  shareElement.passwordName = passwordName;
  document.body.appendChild(shareElement);
  flush();
  return shareElement;
}

function assertVisibleTextContent(element: HTMLElement, expectedText: string) {
  assertTrue(isVisible(element));
  assertEquals(expectedText, element?.textContent!.trim());
}

suite('SharePasswordFlowTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
  });

  test('Has correct loading state', async function() {
    const shareElement = startPasswordShare(/*passwordName=*/ SITE);

    assertEquals(ShareFlowState.FETCHING, shareElement.flowState);
    const dialog =
        shareElement.shadowRoot!.querySelector('share-password-loading-dialog');
    assertTrue(!!dialog);

    await passwordManager.whenCalled('fetchFamilyMembers');
  });

  test('Has correct error state', async function() {
    passwordManager.data.familyFetchResults = makeFamilyFetchResults(
        chrome.passwordsPrivate.FamilyFetchStatus.UNKNOWN_ERROR);
    const shareElement = startPasswordShare();
    await passwordManager.whenCalled('fetchFamilyMembers');
    await flushTasks();

    const dialog =
        shareElement.shadowRoot!.querySelector('share-password-error-dialog');
    assertTrue(!!dialog);

    assertVisibleTextContent(
        dialog.$.header, shareElement.i18n('sharePasswordErrorTitle'));
    assertVisibleTextContent(
        dialog.$.description,
        shareElement.i18n('sharePasswordErrorDescription'));
    assertVisibleTextContent(dialog.$.cancel, shareElement.i18n('cancel'));
    assertVisibleTextContent(
        dialog.$.tryAgain, shareElement.i18n('sharePasswordTryAgain'));
  });

  test('Try again button restarts the flow', async function() {
    passwordManager.data.familyFetchResults = makeFamilyFetchResults(
        chrome.passwordsPrivate.FamilyFetchStatus.UNKNOWN_ERROR);
    const shareElement = startPasswordShare();
    await passwordManager.whenCalled('fetchFamilyMembers');
    await flushTasks();

    const dialog =
        shareElement.shadowRoot!.querySelector('share-password-error-dialog');
    assertTrue(!!dialog);
    dialog.$.tryAgain.click();
    assertEquals(ShareFlowState.FETCHING, shareElement.flowState);
    await passwordManager.whenCalled('fetchFamilyMembers');
  });

  test('Cancel button should hide the error dialog', async function() {
    passwordManager.data.familyFetchResults = makeFamilyFetchResults(
        chrome.passwordsPrivate.FamilyFetchStatus.UNKNOWN_ERROR);
    const shareElement = startPasswordShare();
    await passwordManager.whenCalled('fetchFamilyMembers');
    await flushTasks();

    const shareFlowDone = eventToPromise('share-flow-done', shareElement);

    const dialog =
        shareElement.shadowRoot!.querySelector('share-password-error-dialog');
    assertTrue(!!dialog);
    dialog.$.cancel.click();
    await flushTasks();

    await shareFlowDone;
  });

  test('Has correct no other members state', async function() {
    passwordManager.data.familyFetchResults = makeFamilyFetchResults(
        chrome.passwordsPrivate.FamilyFetchStatus.SUCCESS, /*members=*/[]);
    const shareElement = startPasswordShare();
    await passwordManager.whenCalled('fetchFamilyMembers');
    await flushTasks();

    const dialog = shareElement.shadowRoot!.querySelector(
        'share-password-no-other-family-members-dialog');
    assertTrue(!!dialog);

    assertVisibleTextContent(
        dialog.$.header, shareElement.i18n('shareDialogTitle', SITE));
    assertVisibleTextContent(
        dialog.$.description,
        shareElement.i18n('sharePasswordNoOtherFamilyMembers'));
    assertVisibleTextContent(
        dialog.$.action, shareElement.i18n('sharePasswordGotIt'));
  });

  test('Has correct not a family member state', async function() {
    passwordManager.data.familyFetchResults = makeFamilyFetchResults(
        chrome.passwordsPrivate.FamilyFetchStatus.NO_MEMBERS);
    const shareElement = startPasswordShare();
    await passwordManager.whenCalled('fetchFamilyMembers');
    await flushTasks();

    const dialog = shareElement.shadowRoot!.querySelector(
        'share-password-not-family-member-dialog');
    assertTrue(!!dialog);

    assertVisibleTextContent(
        dialog.$.header, shareElement.i18n('shareDialogTitle', SITE));
    assertVisibleTextContent(
        dialog.$.description,
        shareElement.i18n('sharePasswordNotFamilyMember'));
    assertVisibleTextContent(
        dialog.$.action, shareElement.i18n('sharePasswordGotIt'));
  });

  test('Action button should hide not family member dialog', async function() {
    passwordManager.data.familyFetchResults = makeFamilyFetchResults(
        chrome.passwordsPrivate.FamilyFetchStatus.NO_MEMBERS);
    const shareElement = startPasswordShare();
    await passwordManager.whenCalled('fetchFamilyMembers');
    await flushTasks();

    const shareFlowDone = eventToPromise('share-flow-done', shareElement);

    const dialog = shareElement.shadowRoot!.querySelector(
        'share-password-not-family-member-dialog');
    assertTrue(!!dialog);
    dialog.$.action.click();
    await flushTasks();

    await shareFlowDone;
  });

  test('Action button should hide no other members dialog', async function() {
    passwordManager.data.familyFetchResults = makeFamilyFetchResults(
        chrome.passwordsPrivate.FamilyFetchStatus.SUCCESS, /*members=*/[]);
    const shareElement = startPasswordShare();
    await passwordManager.whenCalled('fetchFamilyMembers');
    await flushTasks();

    const shareFlowDone = eventToPromise('share-flow-done', shareElement);

    const dialog = shareElement.shadowRoot!.querySelector(
        'share-password-no-other-family-members-dialog');
    assertTrue(!!dialog);
    dialog.$.action.click();
    await flushTasks();

    await shareFlowDone;
  });

  test('Cancel button should hide family picker dialog', async function() {
    passwordManager.data.familyFetchResults = makeFamilyFetchResults(
        chrome.passwordsPrivate.FamilyFetchStatus.SUCCESS,
        /*members=*/[makeRecipientInfo()]);
    const shareElement = startPasswordShare();
    await passwordManager.whenCalled('fetchFamilyMembers');
    await flushTasks();

    const shareFlowDone = eventToPromise('share-flow-done', shareElement);

    const dialog = shareElement.shadowRoot!.querySelector(
        'share-password-family-picker-dialog');
    assertTrue(!!dialog);
    dialog.$.cancel.click();
    await flushTasks();

    await shareFlowDone;
  });
});
