// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Route, Router, SettingsRoutes, StoredAccount, SyncPrefs, SyncStatus} from 'chrome://settings/settings.js';
// clang-format on

/**
 * Returns sync prefs with everything synced and no passphrase required.
 */
export function getSyncAllPrefs(): SyncPrefs {
  return {
    appsRegistered: true,
    appsSynced: true,
    appsManaged: false,
    autofillRegistered: true,
    autofillSynced: true,
    autofillManaged: false,
    bookmarksRegistered: true,
    bookmarksSynced: true,
    bookmarksManaged: false,
    encryptAllData: false,
    customPassphraseAllowed: true,
    extensionsRegistered: true,
    extensionsSynced: true,
    extensionsManaged: false,
    passphraseRequired: false,
    passwordsRegistered: true,
    passwordsSynced: true,
    passwordsManaged: false,
    paymentsIntegrationEnabled: true,
    preferencesRegistered: true,
    preferencesSynced: true,
    preferencesManaged: false,
    readingListRegistered: true,
    readingListSynced: true,
    readingListManaged: false,
    savedTabGroupsRegistered: true,
    savedTabGroupsSynced: true,
    savedTabGroupsManaged: false,
    syncAllDataTypes: true,
    tabsRegistered: true,
    tabsSynced: true,
    tabsManaged: false,
    trustedVaultKeysRequired: false,
    themesRegistered: true,
    themesSynced: true,
    themesManaged: false,
    typedUrlsRegistered: true,
    typedUrlsSynced: true,
    typedUrlsManaged: false,
    wifiConfigurationsRegistered: true,
    wifiConfigurationsSynced: true,
    wifiConfigurationsManaged: false,
  };
}

export function getSyncAllPrefsManaged(): SyncPrefs {
  // Make all sync types in the SyncPrefs object disabled by policy.
  return Object.assign(getSyncAllPrefs(), {
    appsSynced: false,
    appsManaged: true,
    autofillSynced: false,
    autofillManaged: true,
    bookmarksSynced: false,
    bookmarksManaged: true,
    extensionsSynced: false,
    extensionsManaged: true,
    passwordsSynced: false,
    passwordsManaged: true,
    paymentsIntegrationEnabled: false,
    preferencesSynced: false,
    preferencesManaged: true,
    readingListSynced: false,
    readingListManaged: true,
    savedTabGroupsSynced: false,
    savedTabGroupsManaged: true,
    tabsSynced: false,
    tabsManaged: true,
    themesSynced: false,
    themesManaged: true,
    typedUrlsSynced: false,
    typedUrlsManaged: true,
    wifiConfigurationsSynced: false,
    wifiConfigurationsManaged: true,
  });
}

export interface SyncRoutes {
  BASIC: Route;
  PEOPLE: Route;
  SYNC: Route;
  SYNC_ADVANCED: Route;
  SIGN_OUT: Route;
  ADVANCED: Route;
  ABOUT: Route;
}

export function setupRouterWithSyncRoutes() {
  const BASIC = new Route('/');
  const PEOPLE = BASIC.createSection('/people', 'people');
  const SYNC = PEOPLE.createChild('/syncSetup');
  const SYNC_ADVANCED = SYNC.createChild('/syncSetup/advanced');

  const SIGN_OUT = BASIC.createChild('/signOut');
  SIGN_OUT.isNavigableDialog = true;

  const routes: SyncRoutes = {
    BASIC,
    PEOPLE,
    SYNC,
    SYNC_ADVANCED,
    SIGN_OUT,
    ADVANCED: new Route('/advanced'),
    ABOUT: new Route('/help'),
  };

  Router.resetInstanceForTesting(
      new Router(routes as unknown as SettingsRoutes));
}

export function simulateSyncStatus(status: SyncStatus|undefined) {
  webUIListenerCallback('sync-status-changed', status);
  flush();
}

export function simulateStoredAccounts(accounts: StoredAccount[]|undefined) {
  webUIListenerCallback('stored-accounts-updated', accounts);
  flush();
}
