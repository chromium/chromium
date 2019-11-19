// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('sync-page-test', function() {
  /** @type {SyncPageElement} */ let syncPage;

  setup(function() {
    PolymerTest.clearBody();

    settings.navigateTo(settings.routes.SYNC);
    syncPage = document.createElement('settings-sync-page');
    document.body.appendChild(syncPage);
    Polymer.dom.flush();
  });

  test('autofocus passphrase input', function() {
    cr.webUIListenerCallback('sync-prefs-changed', {passphraseRequired: false});
    Polymer.dom.flush();
    // Passphrase input is not available when no passphrase is required.
    assertFalse(!!syncPage.$$('#existingPassphraseInput'));

    cr.webUIListenerCallback('sync-prefs-changed', {passphraseRequired: true});
    Polymer.dom.flush();
    // Passphrase input is available and focused when a passphrase is required.
    assertTrue(!!syncPage.$$('#existingPassphraseInput'));
    assertEquals(
        syncPage.$$('#existingPassphraseInput').inputElement,
        syncPage.$$('#existingPassphraseInput').shadowRoot.activeElement);
  });
});
