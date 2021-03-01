// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Router, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {setupRouterWithSyncRoutes} from 'chrome://test/settings/sync_test_util.m.js';
import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';

// clang-format on

suite('sync-page-test', function() {
  /** @type {SyncPageElement} */ let syncPage;

  setup(function() {
    setupRouterWithSyncRoutes();
    PolymerTest.clearBody();
    SyncBrowserProxyImpl.instance_ = new TestSyncBrowserProxy();
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().SYNC);
    syncPage = document.createElement('settings-sync-page');
    document.body.appendChild(syncPage);
    flush();
  });

  test('autofocus passphrase input', function() {
    webUIListenerCallback('sync-prefs-changed', {passphraseRequired: false});
    flush();
    // Passphrase input is not available when no passphrase is required.
    assertFalse(!!syncPage.$$('#existingPassphraseInput'));

    webUIListenerCallback('sync-prefs-changed', {passphraseRequired: true});
    flush();
    // Passphrase input is available and focused when a passphrase is required.
    assertTrue(!!syncPage.$$('#existingPassphraseInput'));
    assertEquals(
        syncPage.$$('#existingPassphraseInput').inputElement,
        syncPage.$$('#existingPassphraseInput').shadowRoot.activeElement);
  });
});
