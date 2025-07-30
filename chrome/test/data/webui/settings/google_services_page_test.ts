// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsGoogleServicesPageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, resetRouterForTesting, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';


suite('GoogleServicesPage', function() {
  let googleServicesPage: SettingsGoogleServicesPageElement;
  let testSyncBrowserProxy: TestSyncBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: true});
    resetRouterForTesting();

    testSyncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(testSyncBrowserProxy);

    googleServicesPage =
        document.createElement('settings-google-services-page');
    testSyncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    document.body.appendChild(googleServicesPage);
    Router.getInstance().navigateTo(routes.GOOGLE_SERVICES);

    return microtasksFinished();
  });

  // Tests that all elements are visible.
  test('ShowCorrectRows', function() {
    assertEquals(
        routes.GOOGLE_SERVICES, Router.getInstance().getCurrentRoute());

    assertTrue(!!googleServicesPage.shadowRoot!.querySelector(
        'settings-personalization-options'));
  });

  // Tests that we navigate back to the people page if the user is syncing.
  test('RedirectToPeopleRoute', async function() {
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });
    await microtasksFinished();

    assertEquals(routes.PEOPLE, Router.getInstance().getCurrentRoute());
  });
});
