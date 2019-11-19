// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('sync_test_util', function() {
  /**
   * Returns sync prefs with everything synced and no passphrase required.
   * @return {!settings.SyncPrefs}
   */
  function getSyncAllPrefs() {
    return {
      appsEnforced: false,
      appsRegistered: true,
      appsSynced: true,
      autofillEnforced: false,
      autofillRegistered: true,
      autofillSynced: true,
      bookmarksEnforced: false,
      bookmarksRegistered: true,
      bookmarksSynced: true,
      encryptAllData: false,
      encryptAllDataAllowed: true,
      enterPassphraseBody: 'Enter custom passphrase.',
      extensionsEnforced: false,
      extensionsRegistered: true,
      extensionsSynced: true,
      fullEncryptionBody: '',
      passphrase: '',
      passphraseRequired: false,
      passwordsEnforced: false,
      passwordsRegistered: true,
      passwordsSynced: true,
      paymentsIntegrationEnabled: true,
      preferencesEnforced: false,
      preferencesRegistered: true,
      preferencesSynced: true,
      setNewPassphrase: false,
      syncAllDataTypes: true,
      tabsEnforced: false,
      tabsRegistered: true,
      tabsSynced: true,
      themesEnforced: false,
      themesRegistered: true,
      themesSynced: true,
      typedUrlsEnforced: false,
      typedUrlsRegistered: true,
      typedUrlsSynced: true,
    };
  }

  /** @param {!settings.SyncStatus} */
  function simulateSyncStatus(status) {
    cr.webUIListenerCallback('sync-status-changed', status);
    Polymer.dom.flush();
  }

  /** @param {Array<!settings.StoredAccount>} */
  function simulateStoredAccounts(accounts) {
    cr.webUIListenerCallback('stored-accounts-updated', accounts);
    Polymer.dom.flush();
  }

  return {
    getSyncAllPrefs: getSyncAllPrefs,
    simulateSyncStatus: simulateSyncStatus,
    simulateStoredAccounts: simulateStoredAccounts,
  };
});
