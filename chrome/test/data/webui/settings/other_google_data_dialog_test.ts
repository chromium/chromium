// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsOtherGoogleDataDialogElement} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement} from 'chrome://settings/settings.js';
import {loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerPage, SignedInState} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// TODO(crbug.com/422340428): Add tests for back & cancel buttons.
suite('OtherGoogleDataDialog', function() {
  let dialog: SettingsOtherGoogleDataDialogElement;
  let passwordManagerProxy: TestPasswordManagerProxy;
  let testOpenWindowProxy: TestOpenWindowProxy;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;


  setup(function() {
    passwordManagerProxy = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManagerProxy);

    testOpenWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(testOpenWindowProxy);

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      showGlicSettings: true,
    });
    return createDialog();
  });

  function createDialog() {
    dialog = document.createElement('settings-other-google-data-dialog');
    document.body.appendChild(dialog);
    return flushTasks();
  }

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

    assertEquals(
        'Settings.DeleteBrowsingData.PasswordManagerLinkClick',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('MyActivityLinkClick', async function() {
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();

    const myActivityLink =
        dialog.shadowRoot!.querySelector<CrLinkRowElement>('#myActivityLink');
    assertTrue(!!myActivityLink);
    assertTrue(isVisible(myActivityLink));
    myActivityLink.click();

    const url = await testOpenWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('deleteBrowsingDataMyActivityUrl'), url);

    assertEquals(
        'Settings.DeleteBrowsingData.MyActivityLinkClick',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('GoogleSearchHistoryLinkClick', async function() {
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();

    const googleSearchHistoryLink =
        dialog.shadowRoot!.querySelector<CrLinkRowElement>(
            '#googleSearchHistoryLink');
    assertTrue(!!googleSearchHistoryLink);
    assertTrue(isVisible(googleSearchHistoryLink));
    googleSearchHistoryLink.click();

    const url = await testOpenWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('deleteBrowsingDataSearchHistoryUrl'), url);

    assertEquals(
        'Settings.DeleteBrowsingData.GoogleSearchHistoryLinkClick',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('NonGoogleSearchHistorySubLabel', async function() {
    setSignedInAndDseState(
        SignedInState.SIGNED_IN, /*isGoogleDse=*/ false,
        /*nonGoogleSearchHistoryString=*/ 'test');
    await flushTasks();

    const nonGoogleSearchHistoryLink =
        dialog.shadowRoot!.querySelector<CrLinkRowElement>(
            '#nonGoogleSearchHistoryLink');
    assertTrue(!!nonGoogleSearchHistoryLink);
    assertTrue(isVisible(nonGoogleSearchHistoryLink));
    assertEquals('test', nonGoogleSearchHistoryLink.subLabel.toString());
  });

  test('MyActivityVisibility', async function() {
    // Case 1: User is signed in, MyActivity link should be visible.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isChildVisible(dialog, '#myActivityLink'));

    // Case 2: User is syncing, MyActivity link should be visible.
    setSignedInAndDseState(SignedInState.SYNCING, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isChildVisible(dialog, '#myActivityLink'));

    // Case 3: User is signed in paused, MyActivity link should be visible.
    setSignedInAndDseState(
        SignedInState.SIGNED_IN_PAUSED, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isChildVisible(dialog, '#myActivityLink'));

    // Case 4: User has web only sign-in in, MyActivity link should be hidden.
    setSignedInAndDseState(
        SignedInState.WEB_ONLY_SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    assertFalse(isChildVisible(dialog, '#myActivityLink'));

    // Case 5: User is signed out, MyActivity link should be hidden.
    setSignedInAndDseState(SignedInState.SIGNED_OUT, /*isGoogleDse=*/ true);
    await flushTasks();
    assertFalse(isChildVisible(dialog, '#myActivityLink'));
  });

  test('SearchHistoryVisibility', async function() {
    // Case 1: User is signed-in and has Google as their DSE, Google search
    // history link should be visible, non-Google search history row should be
    // hidden.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isChildVisible(dialog, '#googleSearchHistoryLink'));
    assertFalse(isChildVisible(dialog, '#nonGoogleSearchHistoryLink'));

    // Case 2: User is syncing and has Google as their DSE, Google search
    // history link should be visible, non-Google search history row should be
    // hidden.
    setSignedInAndDseState(SignedInState.SYNCING, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isChildVisible(dialog, '#googleSearchHistoryLink'));
    assertFalse(isChildVisible(dialog, '#nonGoogleSearchHistoryLink'));

    // Case 3: User is signed-in paused and has Google as their DSE, Google
    // search history link should be visible, non-Google search history row
    // should be hidden.
    setSignedInAndDseState(
        SignedInState.SIGNED_IN_PAUSED, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isChildVisible(dialog, '#googleSearchHistoryLink'));
    assertFalse(isChildVisible(dialog, '#nonGoogleSearchHistoryLink'));

    // Case 4: User is signed-in and does not have Google as their DSE, Google
    // search history link should be hidden, non-Google search history row
    // should be visible.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ false);
    await flushTasks();
    assertFalse(isChildVisible(dialog, '#googleSearchHistoryLink'));
    assertTrue(isChildVisible(dialog, '#nonGoogleSearchHistoryLink'));

    // Case 5: User has web only sign-in and does not have Google as their DSE,
    // Google search history link should be hidden, non-Google search history
    // row should be visible.
    setSignedInAndDseState(SignedInState.SIGNED_OUT, /*isGoogleDse=*/ false);
    await flushTasks();
    assertFalse(isChildVisible(dialog, '#googleSearchHistoryLink'));
    assertTrue(isChildVisible(dialog, '#nonGoogleSearchHistoryLink'));

    // Case 6: User is signed out and has Google as their DSE, Google search
    // history link and non-Google search history row should be hidden.
    setSignedInAndDseState(SignedInState.SIGNED_OUT, /*isGoogleDse=*/ true);
    await flushTasks();
    assertFalse(isChildVisible(dialog, '#googleSearchHistoryLink'));
    assertFalse(isChildVisible(dialog, '#nonGoogleSearchHistoryLink'));
  });

  test('DialogTitle', async function() {
    // Case 1: DSE is Google.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    const title = dialog.shadowRoot!.querySelector('[slot=title]');
    assertTrue(!!title);
    assertEquals(
        loadTimeData.getString('otherGoogleDataTitle'),
        title.textContent.trim());

    // Case 2: DSE is not Google.
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ false);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('otherDataTitle'), title.textContent.trim());
  });

  test('GeminiAppsActivityLinkClick', async function() {
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();

    const geminiAppsActivityLink =
        dialog.shadowRoot!.querySelector<CrLinkRowElement>(
            '#geminiAppsActivityLink');
    assertTrue(!!geminiAppsActivityLink);
    assertTrue(isVisible(geminiAppsActivityLink));
    geminiAppsActivityLink.click();

    const url = await testOpenWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString('myActivityGeminiAppsUrl'), url);

    assertEquals(
        'Settings.DeleteBrowsingData.GeminiAppsActivityLinkClick',
        await testMetricsBrowserProxy.whenCalled('recordAction'));
  });

  test('GeminiAppsActivityVisibility', async function() {
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    assertTrue(isChildVisible(dialog, '#geminiAppsActivityLink'));

    setSignedInAndDseState(SignedInState.SIGNED_OUT, /*isGoogleDse=*/ true);
    await flushTasks();
    assertFalse(isChildVisible(dialog, '#geminiAppsActivityLink'));

    loadTimeData.overrideValues({
      showGlicSettings: false,
    });
    await createDialog();
    setSignedInAndDseState(SignedInState.SIGNED_IN, /*isGoogleDse=*/ true);
    await flushTasks();
    assertFalse(isChildVisible(dialog, '#geminiAppsActivityLink'));
  });
});
