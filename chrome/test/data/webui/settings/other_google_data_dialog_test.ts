// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsOtherGoogleDataDialogElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerPage, SignedInState} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// TODO(crbug.com/422340428): Add tests for back & cancel buttons.
suite('OtherGoogleDataDialog', function() {
  let dialog: SettingsOtherGoogleDataDialogElement;
  let passwordManagerProxy: TestPasswordManagerProxy;
  let testOpenWindowProxy: TestOpenWindowProxy;

  setup(function() {
    passwordManagerProxy = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManagerProxy);

    testOpenWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(testOpenWindowProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-other-google-data-dialog');
    document.body.appendChild(dialog);
    return flushTasks();
  });

  function setSignedInAndDseState(
      signedInState: SignedInState, isGoogleDse: boolean,
      nonGoogleSearchHistoryString?: string) {
    webUIListenerCallback('update-sync-state', {
      isNonGoogleDse: !isGoogleDse,
      nonGoogleSearchHistoryString: nonGoogleSearchHistoryString,
    });
    webUIListenerCallback('sync-status-changed', {
      signedInState: signedInState,
    });
  }

  test('PasswordManagerLinkClick', async function() {
    dialog.$.passwordManagerLink.click();

    assertEquals(
        PasswordManagerPage.PASSWORDS,
        await passwordManagerProxy.whenCalled('showPasswordManager'));
  });

  test('MyActivityLinkClick', async function() {
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    dialog.$.myActivityLink.click();

    const url = await testOpenWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('deleteBrowsingDataMyActivityUrl'), url);
  });

  test('GoogleSearchHistoryLinkClick', async function() {
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    dialog.$.googleSearchHistoryLink.click();

    const url = await testOpenWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('deleteBrowsingDataSearchHistoryUrl'), url);
  });

  test('NonGoogleSearchHistorySubLabel', async function() {
    setSignedInAndDseState(
        SignedInState.SIGNED_IN, /*isGoogleDse=*/ true,
        /*nonGoogleSearchHistoryString=*/ 'test');
    await flushTasks();

    const subLabel =
        dialog.$.nonGoogleSearchHistoryLink.querySelector('.secondary');
    assertTrue(!!subLabel);
    assertEquals('test', subLabel.textContent!.trim());
  });

  test('MyActivityVisibility', async function() {
    // Case 1: User is signed in, MyActivity link should be visible.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isVisible(dialog.$.myActivityLink));

    // Case 2: User is syncing, MyActivity link should be visible.
    setSignedInAndDseState(SignedInState.SYNCING, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isVisible(dialog.$.myActivityLink));

    // Case 3: User is signed in paused, MyActivity link should be visible.
    setSignedInAndDseState(
        SignedInState.SIGNED_IN_PAUSED, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isVisible(dialog.$.myActivityLink));

    // Case 4: User has web only sign-in in, MyActivity link should be hidden.
    setSignedInAndDseState(
        SignedInState.WEB_ONLY_SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    assertFalse(isVisible(dialog.$.myActivityLink));

    // Case 5: User is signed out, MyActivity link should be hidden.
    setSignedInAndDseState(SignedInState.SIGNED_OUT, /*isGoogleDse=*/ true);
    await flushTasks();
    assertFalse(isVisible(dialog.$.myActivityLink));
  });

  test('SearchHistoryVisibility', async function() {
    // Case 1: User is signed-in and has Google as their DSE, Google search
    // history link should be visible, non-Google search history row should be
    // hidden.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();

    assertTrue(isVisible(dialog.$.googleSearchHistoryLink));
    assertFalse(isVisible(dialog.$.nonGoogleSearchHistoryLink));

    // Case 2: User is syncing and has Google as their DSE, Google search
    // history link should be visible, non-Google search history row should be
    // hidden.
    setSignedInAndDseState(SignedInState.SYNCING, /*isGoogleDse=*/ true);
    await flushTasks();

    assertTrue(isVisible(dialog.$.googleSearchHistoryLink));
    assertFalse(isVisible(dialog.$.nonGoogleSearchHistoryLink));

    // Case 3: User is signed-in paused and has Google as their DSE, Google
    // search history link should be visible, non-Google search history row
    // should be hidden.
    setSignedInAndDseState(
        SignedInState.SIGNED_IN_PAUSED, /*isGoogleDse=*/ true);
    await flushTasks();

    assertTrue(isVisible(dialog.$.googleSearchHistoryLink));
    assertFalse(isVisible(dialog.$.nonGoogleSearchHistoryLink));

    // Case 4: User is signed-in and does not have Google as their DSE, Google
    // search history link should be hidden, non-Google search history row
    // should be visible.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ false);
    await flushTasks();

    assertFalse(isVisible(dialog.$.googleSearchHistoryLink));
    assertTrue(isVisible(dialog.$.nonGoogleSearchHistoryLink));

    // Case 5: User has web only sign-in and does not have Google as their DSE,
    // Google search history link should be hidden, non-Google search history
    // row should be visible.
    setSignedInAndDseState(SignedInState.SIGNED_OUT, /*isGoogleDse=*/ false);
    await flushTasks();

    assertFalse(isVisible(dialog.$.googleSearchHistoryLink));
    assertTrue(isVisible(dialog.$.nonGoogleSearchHistoryLink));

    // Case 6: User is signed out and has Google as their DSE, Google search
    // history link and non-Google search history row should be hidden.
    setSignedInAndDseState(SignedInState.SIGNED_OUT, /*isGoogleDse=*/ true);
    await flushTasks();

    assertFalse(isVisible(dialog.$.googleSearchHistoryLink));
    assertFalse(isVisible(dialog.$.nonGoogleSearchHistoryLink));
  });

  test('DialogTitle', async function() {
    // Case 1: DSE is Google.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    const title = dialog.shadowRoot!.querySelector('[slot=title]');
    assertTrue(!!title);
    assertEquals(
        loadTimeData.getString('otherGoogleDataTitle'),
        title.textContent!.trim());

    // Case 2: DSE is not Google.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ false);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('otherDataTitle'), title.textContent!.trim());
  });

  test('LinkRowsCssClass', async function() {
    // Case 1: User is signed in and Google is DSE, passwords > Google search
    // history > my activity rows should be shown in this order.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(
        dialog.$.passwordManagerLink.classList.contains('first-link-row'));
    assertTrue(
        dialog.$.googleSearchHistoryLink.classList.contains('middle-link-row'));
    assertTrue(dialog.$.myActivityLink.classList.contains('last-link-row'));
    assertFalse(isVisible(dialog.$.nonGoogleSearchHistoryLink));

    // Case 2: User is signed in and Google is not the DSE, passwords  > my
    // activity > non Google search history rows should be shown in this order.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ false);
    await flushTasks();
    assertTrue(
        dialog.$.passwordManagerLink.classList.contains('first-link-row'));
    assertFalse(isVisible(dialog.$.googleSearchHistoryLink));
    assertTrue(dialog.$.myActivityLink.classList.contains('middle-link-row'));
    assertTrue(dialog.$.nonGoogleSearchHistoryLink.classList.contains(
        'last-link-row'));

    // Case 3: User is not signed in and Google is not the DSE, passwords  > non
    // Google search history rows should be shown in this order.
    setSignedInAndDseState(SignedInState.SIGNED_OUT, /*isGoogleDse=*/ false);
    await flushTasks();
    assertTrue(
        dialog.$.passwordManagerLink.classList.contains('first-link-row'));
    assertFalse(isVisible(dialog.$.googleSearchHistoryLink));
    assertFalse(isVisible(dialog.$.myActivityLink));
    assertTrue(dialog.$.nonGoogleSearchHistoryLink.classList.contains(
        'last-link-row'));

    // Case 4: User is not signed in and Google is the DSE, only passwords row
    // should be shown.
    setSignedInAndDseState(SignedInState.SIGNED_OUT, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(
        dialog.$.passwordManagerLink.classList.contains('only-link-row'));
    assertFalse(isVisible(dialog.$.googleSearchHistoryLink));
    assertFalse(isVisible(dialog.$.myActivityLink));
    assertFalse(isVisible(dialog.$.nonGoogleSearchHistoryLink));
  });
});
