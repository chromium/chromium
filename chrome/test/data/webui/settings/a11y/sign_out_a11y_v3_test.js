// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {routes, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {simulateSyncStatus} from 'chrome://test/settings/sync_test_util.m.js';
import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';
import {waitBeforeNextRender} from 'chrome://test/test_util.m.js';

// Set the URL of the page to render to load the correct view upon
// injecting settings-ui without attaching listeners.
window.history.pushState('object or string', 'Test', routes.PEOPLE.path);

const browserProxy = new TestSyncBrowserProxy();
SyncBrowserProxyImpl.instance_ = browserProxy;

const settingsUi = document.createElement('settings-ui');
document.body.appendChild(settingsUi);
flush();

const peoplePage = settingsUi.$$('settings-main')
                       .$$('settings-basic-page')
                       .$$('settings-people-page');
assertTrue(!!peoplePage);

simulateSyncStatus({
  signedIn: false,
  signinAllowed: true,
  syncSystemEnabled: true,
  disabled: false,
});

let parent = null;

browserProxy.getSyncStatus()
    .then(syncStatus => {
      // Navigate to the sign out dialog.
      flush();

      parent = peoplePage.$$('settings-sync-account-control');
      parent.syncStatus = {
        firstSetupInProgress: false,
        signedIn: true,
        signedInUsername: 'bar@bar.com',
        statusAction: StatusAction.NO_ACTION,
        hasError: false,
        disabled: false,
      };
      return waitBeforeNextRender(parent);
    })
    .then(() => {
      const disconnectButton = parent.$$('#turn-off');
      assertTrue(!!disconnectButton);
      disconnectButton.click();
      document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
    });
