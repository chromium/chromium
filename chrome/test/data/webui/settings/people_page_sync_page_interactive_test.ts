// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrInputElement, SettingsSyncPageElement} from 'chrome://settings/lazy_load.js';
import {Router, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {setupRouterWithSyncRoutes, SyncRoutes} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

suite('sync-page-test', function() {
  let syncPage: SettingsSyncPageElement;

  setup(function() {
    setupRouterWithSyncRoutes();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    SyncBrowserProxyImpl.setInstance(new TestSyncBrowserProxy());
    const router = Router.getInstance();
    router.navigateTo((router.getRoutes() as SyncRoutes).SYNC);
    syncPage = document.createElement('settings-sync-page');
    document.body.appendChild(syncPage);
    flush();
  });

  test('autofocus passphrase input', function() {
    syncPage.syncStatus = {
      signedIn: true,
      disabled: false,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    };
    webUIListenerCallback('sync-prefs-changed', {passphraseRequired: false});
    flush();
    // Passphrase input is not available when no passphrase is required.
    assertFalse(
        !!syncPage.shadowRoot!.querySelector('#existingPassphraseInput'));

    webUIListenerCallback('sync-prefs-changed', {passphraseRequired: true});
    flush();
    // Passphrase input is available and focused when a passphrase is required.
    assertTrue(
        !!syncPage.shadowRoot!.querySelector('#existingPassphraseInput'));
    assertEquals(
        syncPage.shadowRoot!
            .querySelector<CrInputElement>(
                '#existingPassphraseInput')!.inputElement,
        syncPage.shadowRoot!.querySelector('#existingPassphraseInput')!
            .shadowRoot!.activeElement);
  });
});
