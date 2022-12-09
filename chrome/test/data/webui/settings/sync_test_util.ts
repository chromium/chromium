// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Route, Router, SettingsRoutes, StoredAccount, SyncStatus} from 'chrome://settings/settings.js';
// clang-format on

interface SyncAllPrefs {
  appsRegistered: boolean;
  appsSynced: boolean;
  autofillRegistered: boolean;
  autofillSynced: boolean;
  bookmarksRegistered: boolean;
  bookmarksSynced: boolean;
  encryptAllData: boolean;
  customPassphraseAllowed: boolean;
  extensionsRegistered: boolean;
  extensionsSynced: boolean;
  passphraseRequired: boolean;
  passwordsRegistered: boolean;
  passwordsSynced: boolean;
  paymentsIntegrationEnabled: boolean;
  preferencesRegistered: boolean;
  preferencesSynced: boolean;
  readingListRegistered: boolean;
  readingListSynced: boolean;
  savedTabGroupsRegistered: boolean;
  savedTabGroupsSynced: boolean;
  syncAllDataTypes: boolean;
  tabsRegistered: boolean;
  tabsSynced: boolean;
  themesRegistered: boolean;
  themesSynced: boolean;
  typedUrlsRegistered: boolean;
  typedUrlsSynced: boolean;
  explicitPassphraseTime?: string;
}

/**
 * Returns sync prefs with everything synced and no passphrase required.
 */
export function getSyncAllPrefs(): SyncAllPrefs {
  return {
    appsRegistered: true,
    appsSynced: true,
    autofillRegistered: true,
    autofillSynced: true,
    bookmarksRegistered: true,
    bookmarksSynced: true,
    encryptAllData: false,
    customPassphraseAllowed: true,
    extensionsRegistered: true,
    extensionsSynced: true,
    passphraseRequired: false,
    passwordsRegistered: true,
    passwordsSynced: true,
    paymentsIntegrationEnabled: true,
    preferencesRegistered: true,
    preferencesSynced: true,
    readingListRegistered: true,
    readingListSynced: true,
    savedTabGroupsRegistered: true,
    savedTabGroupsSynced: true,
    syncAllDataTypes: true,
    tabsRegistered: true,
    tabsSynced: true,
    themesRegistered: true,
    themesSynced: true,
    typedUrlsRegistered: true,
    typedUrlsSynced: true,
  };
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
