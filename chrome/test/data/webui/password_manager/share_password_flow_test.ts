// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {ShareFlowState, SharePasswordFlowElement} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

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
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('Has correct loading state', async function() {
    const shareElement = startPasswordShare(/*passwordName=*/ SITE);
    assertEquals(ShareFlowState.FETCHING, shareElement.flowState);
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

  test('Has correct error state', async function() {
    const shareElement = startPasswordShare();
    await flushTasks();
    shareElement.flowState = ShareFlowState.ERROR;
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
    const shareElement = startPasswordShare();
    await flushTasks();
    shareElement.flowState = ShareFlowState.ERROR;
    await flushTasks();

    const dialog =
        shareElement.shadowRoot!.querySelector('share-password-error-dialog');
    assertTrue(!!dialog);
    dialog.$.tryAgain.click();
    // TODO(crbug/1445526): Update the test after passwords private api changes.
    await flushTasks();

    assertEquals(ShareFlowState.FETCHING, shareElement.flowState);
    const loadingDialog =
        shareElement.shadowRoot!.querySelector('share-password-loading-dialog');
    assertTrue(!!loadingDialog);
  });

  test('Cancel button should hide the error dialog', async function() {
    const shareElement = startPasswordShare();
    await flushTasks();
    shareElement.flowState = ShareFlowState.ERROR;
    await flushTasks();

    const shareFlowDone = eventToPromise('share-flow-done', shareElement);

    const dialog =
        shareElement.shadowRoot!.querySelector('share-password-error-dialog');
    assertTrue(!!dialog);
    dialog.$.cancel.click();
    await flushTasks();

    await shareFlowDone;
  });
});
