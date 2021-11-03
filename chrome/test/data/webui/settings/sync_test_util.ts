// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Route, Router, StoredAccount, SyncStatus} from 'chrome://settings/settings.js';
// clang-format on

/**
 * Returns sync prefs with everything synced and no passphrase required.
 */
export function getSyncAllPrefs(): {[key: string]: boolean} {
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
    syncAllDataTypes: true,
    tabsRegistered: true,
    tabsSynced: true,
    themesRegistered: true,
    themesSynced: true,
    typedUrlsRegistered: true,
    typedUrlsSynced: true,
  };
}

export function setupRouterWithSyncRoutes() {
  const BASIC = new Route('/');
  const PEOPLE = BASIC.createSection('/people', 'people');
  const SYNC = PEOPLE.createChild('/syncSetup');
  const SYNC_ADVANCED = SYNC.createChild('/syncSetup/advanced');

  const SIGN_OUT = BASIC.createChild('/signOut');
  SIGN_OUT.isNavigableDialog = true;

  const routes = {
    BASIC,
    PEOPLE,
    SYNC,
    SYNC_ADVANCED,
    SIGN_OUT,
    ADVANCED: new Route('/advanced'),
    ABOUT: new Route('/help'),
  };

  Router.resetInstanceForTesting(new Router(routes));
}

export function simulateSyncStatus(status: SyncStatus) {
  webUIListenerCallback('sync-status-changed', status);
  flush();
}

export function simulateStoredAccounts(accounts: StoredAccount[]) {
  webUIListenerCallback('stored-accounts-updated', accounts);
  flush();
}
