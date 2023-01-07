// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {routes, SettingsSyncAccountControlElement, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {simulateSyncStatus} from '../sync_test_util.js';
import {TestSyncBrowserProxy} from '../test_sync_browser_proxy.js';

// Set the URL of the page to render to load the correct view upon
// injecting settings-ui without attaching listeners.
window.history.pushState('object or string', 'Test', routes.PEOPLE.path);

const browserProxy: TestSyncBrowserProxy = new TestSyncBrowserProxy();
SyncBrowserProxyImpl.setInstance(browserProxy);

const settingsUi = document.createElement('settings-ui');
document.body.appendChild(settingsUi);
flush();

const peoplePage =
    settingsUi.shadowRoot!.querySelector('settings-main')!.shadowRoot!
        .querySelector('settings-basic-page')!.shadowRoot!.querySelector(
            'settings-people-page');
assertTrue(!!peoplePage);

simulateSyncStatus({
  signedIn: false,
  statusAction: StatusAction.NO_ACTION,
  syncSystemEnabled: true,
  disabled: false,
});

let parent: SettingsSyncAccountControlElement;

browserProxy.getSyncStatus()
    .then(() => {
      // Navigate to the sign out dialog.
      flush();

      parent = peoplePage.shadowRoot!.querySelector(
          'settings-sync-account-control')!;
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
      const disconnectButton =
          parent.shadowRoot!.querySelector<HTMLElement>('#turn-off');
      assertTrue(!!disconnectButton);
      disconnectButton.click();
      document.dispatchEvent(new CustomEvent('a11y-setup-complete'));
    });
