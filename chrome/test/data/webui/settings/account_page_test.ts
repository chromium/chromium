// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsAccountPageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';


suite('AccountPageTests', function() {
  let accountSettingsPage: SettingsAccountPageElement;
  let testSyncBrowserProxy: TestSyncBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: true});

    testSyncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(testSyncBrowserProxy);

    accountSettingsPage = createSettingsAccountPageElement();
    Router.getInstance().navigateTo(routes.ACCOUNT);

    return microtasksFinished();
  });

  function createSettingsAccountPageElement(): SettingsAccountPageElement {
    const element = document.createElement('settings-account-page');
    testSyncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    document.body.appendChild(element);
    return element;
  }

  // Tests that all elements are visible.
  test('ShowCorrectRows', function() {
    assertEquals(routes.ACCOUNT, Router.getInstance().getCurrentRoute());

    assertTrue(!!accountSettingsPage.shadowRoot!.querySelector(
        'settings-sync-account-control'));
    assertTrue(!!accountSettingsPage.shadowRoot!.querySelector(
        'settings-sync-controls'));
  });

  // Tests that we navigate back to the people page if the user is not signed
  // in.
  test('accountSettingsPageUnavailableWhenNotSignedIn', async function() {
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    });
    await microtasksFinished();

    assertEquals(routes.PEOPLE, Router.getInstance().getCurrentRoute());
  });
});
