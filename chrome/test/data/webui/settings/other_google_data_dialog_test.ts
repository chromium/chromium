// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsOtherGoogleDataDialogElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl, PasswordManagerImpl, PasswordManagerPage} from 'chrome://settings/settings.js';
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
    dialog.isSignedIn = true;
    dialog.isGoogleDse = true;
    document.body.appendChild(dialog);
    return flushTasks();
  });

  test('PasswordManagerLinkClick', async function() {
    dialog.$.passwordManagerLink.click();

    assertEquals(
        PasswordManagerPage.PASSWORDS,
        await passwordManagerProxy.whenCalled('showPasswordManager'));
  });

  test('MyActivityLinkClick', async function() {
    dialog.$.myActivityLink.click();

    const url = await testOpenWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('deleteBrowsingDataMyActivityUrl'), url);
  });

  test('SearchHistoryLinkClick', async function() {
    dialog.$.searchHistoryLink.click();

    const url = await testOpenWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('deleteBrowsingDataSearchHistoryUrl'), url);
  });

  test('MyActivityVisibility', async function() {
    // Case 1: User is signed in, MyActivity link should be visible.
    assertTrue(isVisible(dialog.$.myActivityLink));

    // Case 2: User is signed out, MyActivity link should be hidden.
    dialog.isSignedIn = false;
    await flushTasks();
    assertFalse(isVisible(dialog.$.myActivityLink));
  });

  test('SearchHistoryVisibility', async function() {
    // Case 1: User is signed in and DSE is Google, Search History link should
    // be visible.
    assertTrue(isVisible(dialog.$.searchHistoryLink));

    // Case 2: User is signed in and DSE is not Google, Search History link
    // should be visible.
    dialog.isGoogleDse = false;
    await flushTasks();
    assertTrue(isVisible(dialog.$.searchHistoryLink));

    // Case 3: User is not signed in and DSE is not Google, Search History link
    // should be visible.
    dialog.isSignedIn = false;
    await flushTasks();
    assertTrue(isVisible(dialog.$.searchHistoryLink));

    // Case 4: User is not signed in and DSE is Google, Search History link
    // should be hidden.
    dialog.isGoogleDse = true;
    await flushTasks();
    assertFalse(isVisible(dialog.$.searchHistoryLink));
  });
});
