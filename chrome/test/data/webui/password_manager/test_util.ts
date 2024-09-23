// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface PasswordCheckParams {
  state?: chrome.passwordsPrivate.PasswordCheckState;
  totalNumber?: number;
  checked?: number;
  remaining?: number;
  lastCheck?: string;
}

export function makePasswordCheckStatus(params: PasswordCheckParams):
    chrome.passwordsPrivate.PasswordCheckStatus {
  return {
    state: params.state || chrome.passwordsPrivate.PasswordCheckState.IDLE,
    totalNumberOfPasswords: params.totalNumber,
    alreadyProcessed: params.checked,
    remainingInQueue: params.remaining,
    elapsedTimeSinceLastCheck: params.lastCheck,
  };
}

export function makeFamilyFetchResults(
    status?: chrome.passwordsPrivate.FamilyFetchStatus,
    members?: chrome.passwordsPrivate.RecipientInfo[]):
    chrome.passwordsPrivate.FamilyFetchResults {
  return {
    status: status || chrome.passwordsPrivate.FamilyFetchStatus.SUCCESS,
    familyMembers: members || [],
  };
}

export function makeRecipientInfo(isEligible: boolean = true):
    chrome.passwordsPrivate.RecipientInfo {
  return {
    userId: 'user-id',
    email: 'user@example.com',
    displayName: 'New User',
    profileImageUrl: 'data://image/url',
    isEligible: isEligible,
  };
}

export interface PasswordEntryParams {
  isPasskey?: boolean;
  url?: string;
  username?: string;
  displayName?: string;
  password?: string;
  federationText?: string;
  id?: number;
  inAccountStore?: boolean;
  inProfileStore?: boolean;
  note?: string;
  changePasswordUrl?: string;
  affiliatedDomains?: chrome.passwordsPrivate.DomainInfo[];
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
  const url = params.url || 'www.foo.com';
  const domain = {
    name: url,
    url: `https://${url}/login`,
    signonRealm: `https://${url}/login`,
  };
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
    isPasskey: params.isPasskey || false,
    username: username,
    displayName: params.displayName,
    federationText: params.federationText,
    id: id,
    storedIn: storeType,
    note: note,
    changePasswordUrl: params.changePasswordUrl,
    password: params.password || '',
    affiliatedDomains: params.affiliatedDomains || [domain],
    creationTime: params.isPasskey ? 1000000000 : undefined,
  };
}

export interface CredentialGroupParams {
  name?: string;
  icon?: string;
  credentials?: chrome.passwordsPrivate.PasswordUiEntry[];
}

export function createCredentialGroup(params?: CredentialGroupParams):
    chrome.passwordsPrivate.CredentialGroup {
  params = params || {};
  return {
    name: params.name || '',
    iconUrl: params.icon || '',
    entries: params.credentials || [],
  };
}

/**
 * Creates a single item for the list of password blockedSites. If no |id| is
 * passed, it is set to a default, value so this should probably not be done in
 * tests with multiple entries (|id| is unique).
 */
export function createBlockedSiteEntry(
    url?: string, id?: number): chrome.passwordsPrivate.ExceptionEntry {
  url = url || 'www.foo.com';
  id = id || 42;
  return {
    urls: {
      signonRealm: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    id: id,
  };
}

export function makePasswordManagerPrefs() {
  return {
    credentials_enable_service: {
      key: 'credentials_enable_service',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    credentials_enable_autosignin: {
      key: 'credentials_enable_autosignin',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    profile: {
      password_dismiss_compromised_alert: {
        key: 'profile.password_dismiss_compromised_alert',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    },
    password_manager: {
      // <if expr="is_win or is_macosx or is_chromeos">
      biometric_authentication_filling: {
        key: 'password_manager.biometric_authentication_filling',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      // </if>
      password_sharing_enabled: {
        key: 'password_manager.password_sharing_enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    },
  };
}

export interface InsecureCredentialsParams {
  url?: string;
  username?: string;
  password?: string;
  types?: chrome.passwordsPrivate.CompromiseType[];
  id?: number;
  elapsedMinSinceCompromise?: number;
  isMuted?: boolean;
}

/**
 * Creates a new insecure credential.
 */
export function makeInsecureCredential(params: InsecureCredentialsParams):
    chrome.passwordsPrivate.PasswordUiEntry {
  // Generate fake data if param is undefined.
  params = params || {};
  const url = params.url !== undefined ? params.url : 'www.foo.com';
  const username = params.username !== undefined ? params.username : 'user';
  const id = params.id !== undefined ? params.id : 42;
  const elapsedMinSinceCompromise = params.elapsedMinSinceCompromise || 0;
  const types = params.types || [];
  const compromisedInfo = {
    compromiseTime: Date.now() - (elapsedMinSinceCompromise * 60000),
    elapsedTimeSinceCompromise: `${elapsedMinSinceCompromise} minutes ago`,
    compromiseTypes: types,
    isMuted: params.isMuted ?? false,
  };
  return {
    affiliatedDomains: [{
      name: url,
      url: `https://${url}/login`,
      signonRealm: `https://${url}/login`,
    }],
    isPasskey: false,
    id: id || 0,
    storedIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
    changePasswordUrl: `https://${url}/`,
    username: username,
    password: params.password,
    note: '',
    compromisedInfo: types.length ? compromisedInfo : undefined,
    creationTime: undefined,
  };
}

export function createAffiliatedDomain(domain: string):
    chrome.passwordsPrivate.DomainInfo {
  return {
    name: domain,
    url: `https://${domain}/login`,
    signonRealm: `https://${domain}/login`,
  };
}
