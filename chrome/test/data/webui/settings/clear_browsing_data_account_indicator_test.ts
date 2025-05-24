// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsClearBrowsingDataAccountIndicator} from 'chrome://settings/lazy_load.js';
import {SignedInState, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {simulateStoredAccounts} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

suite('DeleteBrowsingDataAccountIndicator', function() {
  let indicator: SettingsClearBrowsingDataAccountIndicator;
  let testSyncBrowserProxy: TestSyncBrowserProxy;

  setup(async function() {
    testSyncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(testSyncBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    indicator = document.createElement(
        'settings-clear-browsing-data-account-indicator');
    document.body.appendChild(indicator);

    await testSyncBrowserProxy.whenCalled('getSyncStatus');
    await testSyncBrowserProxy.whenCalled('getStoredAccounts');
    await flushTasks();
  });

  test('IndicatorVisibilityWithStoredAccounts', async function() {
    simulateStoredAccounts([
      {
        fullName: 'fooName',
        givenName: 'foo',
        email: 'foo@foo.com',
      },
      {
        fullName: 'barName',
        givenName: 'bar',
        email: 'bar@bar.com',
      },
    ]);

    function checkAccountIndicatorVisibility() {
      const avatarRow =
          indicator.shadowRoot!.querySelector<HTMLElement>('#avatarRow');
      return isVisible(avatarRow);
    }

    function checkShownAccount() {
      // Shown account must match the primary account which is the first account
      // in the StoredAccounts list.
      const userInfo =
          indicator.shadowRoot!.querySelector<HTMLElement>('#userInfo')!;
      assertTrue(!!userInfo);
      const title = userInfo.children[0]!.textContent!;
      const subtitle = userInfo.children[1]!.textContent!;
      assertEquals(title.trim(), 'fooName');
      assertEquals(subtitle.trim(), 'foo@foo.com');
    }

    // Signed out: Account indicator is hidden.
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SIGNED_OUT,
      hasError: false,
    });
    await flushTasks();
    assertFalse(checkAccountIndicatorVisibility());

    // Signin pending: Account indicator is hidden.
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SIGNED_IN_PAUSED,
      hasError: false,
    });
    await flushTasks();
    assertFalse(checkAccountIndicatorVisibility());

    // Web only signin: Account indicator is hidden.
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.WEB_ONLY_SIGNED_IN,
      hasError: false,
    });
    await flushTasks();
    assertFalse(checkAccountIndicatorVisibility());

    // Signed in: Account indicator is Visible.
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SIGNED_IN,
      hasError: false,
    });
    await flushTasks();
    assertTrue(checkAccountIndicatorVisibility());
    checkShownAccount();

    // Syncing: Account indicator is Visible.
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SYNCING,
      hasError: false,
    });
    await flushTasks();
    assertTrue(checkAccountIndicatorVisibility());
    checkShownAccount();
  });

  test('IndicatorVisibilityNoStoredAccounts', async function() {
    simulateStoredAccounts([]);
    await flushTasks();

    assertFalse(
        !!indicator.shadowRoot!.querySelector<HTMLElement>('#avatarRow'));
  });
});
