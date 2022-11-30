// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function makePasswordCheckStatus(
    state?: chrome.passwordsPrivate.PasswordCheckState, checked?: number,
    remaining?: number,
    lastCheck?: string): chrome.passwordsPrivate.PasswordCheckStatus {
  return {
    state: state || chrome.passwordsPrivate.PasswordCheckState.IDLE,
    alreadyProcessed: checked,
    remainingInQueue: remaining,
    elapsedTimeSinceLastCheck: lastCheck,
  };
}

export interface PasswordEntryParams {
  url?: string;
  username?: string;
  federationText?: string;
  id?: number;
  inAccountStore?: boolean;
  inProfileStore?: boolean;
  isAndroidCredential?: boolean;
  note?: string;
}

/**
 * Creates a single item for the list of passwords, in the format sent by the
 * password manager native code. If no |params.id| is passed, it is set to a
 * default, value so this should probably not be done in tests with multiple
 * entries (|params.id| is unique). If no |params.frontendId| is passed, it is
 * set to the same value set for |params.id|.
 */
export function createPasswordEntry(params?: PasswordEntryParams):
    chrome.passwordsPrivate.PasswordUiEntry {
  // Generate fake data if param is undefined.
  params = params || {};
  const url = params.url !== undefined ? params.url : 'www.foo.com';
  const username = params.username !== undefined ? params.username : 'user';
  const id = params.id !== undefined ? params.id : 42;
  // Fallback to device store if no parameter provided.
  let storeType: chrome.passwordsPrivate.PasswordStoreSet =
      chrome.passwordsPrivate.PasswordStoreSet.DEVICE;

  if (params.inAccountStore && params.inProfileStore) {
    storeType = chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT;
  } else if (params.inAccountStore) {
    storeType = chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
  } else if (params.inProfileStore) {
    storeType = chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
  }
  const note = params.note || '';

  return {
    urls: {
      signonRealm: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    username: username,
    federationText: params.federationText,
    id: id,
    storedIn: storeType,
    isAndroidCredential: params.isAndroidCredential || false,
    note: note,
    password: '',
    hasStartableScript: false,
  };
}

export function makePasswordManagerPrefs():
    chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: 'credentials_enable_service',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'credentials_enable_autosignin',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'profile.password_dismiss_compromised_alert',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
  ];
}
