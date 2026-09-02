// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {CrInputElement, SettingsSyncPageElement} from 'chrome://settings/lazy_load.js';
import {Router, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

suite('sync-page-test', function() {
  let syncPage: SettingsSyncPageElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const browserProxy = new TestSyncBrowserProxy();
    browserProxy.testSyncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    SyncBrowserProxyImpl.setInstance(browserProxy);
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().SYNC);
    syncPage = document.createElement('settings-sync-page');
    document.body.appendChild(syncPage);
  });

  test('autofocus passphrase input', async function() {
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SYNCING,
      disabled: false,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    });
    webUIListenerCallback('sync-prefs-changed', {passphraseRequired: false});
    await microtasksFinished();
    // Passphrase input is not available when no passphrase is required.
    assertFalse(
        !!syncPage.shadowRoot.querySelector('#existingPassphraseInput'));

    webUIListenerCallback('sync-prefs-changed', {passphraseRequired: true});
    await microtasksFinished();
    // Passphrase input is available and focused when a passphrase is required.
    assertTrue(!!syncPage.shadowRoot.querySelector('#existingPassphraseInput'));
    assertEquals(
        syncPage.shadowRoot
            .querySelector<CrInputElement>(
                '#existingPassphraseInput')!.inputElement,
        syncPage.shadowRoot.querySelector('#existingPassphraseInput')!
            .shadowRoot!.activeElement);
  });
});
