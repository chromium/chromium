// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PasswordManagerProxy. */

import {AccountStorageOptInStateChangedListener, BlockedSite, BlockedSitesListChangedListener, CredentialsChangedListener, PasswordCheckInteraction, PasswordCheckStatusChangedListener, PasswordManagerAuthTimeoutListener, PasswordManagerProxy, PasswordsFileExportProgressListener, PasswordViewPageInteractions} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {makeFamilyFetchResults, makePasswordCheckStatus} from './test_util.js';

/**
 * Test implementation
 */
export class TestPasswordManagerProxy extends TestBrowserProxy implements
    PasswordManagerProxy {
  data: {
    blockedSites: BlockedSite[],
    checkStatus: chrome.passwordsPrivate.PasswordCheckStatus,
    credentialWithReusedPassword: chrome.passwordsPrivate.PasswordUiEntryList[],
    familyFetchResults: chrome.passwordsPrivate.FamilyFetchResults,
    groups: chrome.passwordsPrivate.CredentialGroup[],
    insecureCredentials: chrome.passwordsPrivate.PasswordUiEntry[],
    isOptedInAccountStorage: boolean,
    isAccountStorageDefault: boolean,
    passwords: chrome.passwordsPrivate.PasswordUiEntry[],
  };

  listeners: {
    accountStorageOptInStateListener: AccountStorageOptInStateChangedListener|
    null,
    blockedSitesListChangedListener: BlockedSitesListChangedListener|null,
    savedPasswordListChangedListener: CredentialsChangedListener|null,
    passwordCheckStatusListener: PasswordCheckStatusChangedListener|null,
    insecureCredentialsListener: CredentialsChangedListener|null,
    passwordsFileExportProgressListener: PasswordsFileExportProgressListener|
    null,
    passwordManagerAuthTimeoutListener: PasswordManagerAuthTimeoutListener|null,
  };

  private requestCredentialsDetailsResponse_:
      chrome.passwordsPrivate.PasswordUiEntry[]|null = null;

  private importResults_: chrome.passwordsPrivate.ImportResults = {
    status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
    numberImported: 0,
    displayedEntries: [],
    fileName: '',
  };

  constructor() {
    super([
      'addPassword',
      'changeCredential',
      'cancelExportPasswords',
      'continueImport',
      'dismissSafetyHubPasswordMenuNotification',
      'exportPasswords',
      'extendAuthValidity',
      'fetchFamilyMembers',
      'importPasswords',
      'isAccountStoreDefault',
      'isOptedInForAccountStorage',
      'getBlockedSitesList',
      'getCredentialGroups',
      'getCredentialsWithReusedPassword',
      'getInsecureCredentials',
      'getPasswordCheckStatus',
      'getSavedPasswordList',
      'getUrlCollection',
      'movePasswordsToAccount',
      'muteInsecureCredential',
      'optInForAccountStorage',
      'recordPasswordCheckInteraction',
      'recordPasswordViewInteraction',
      'removeBlockedSite',
      'removeCredential',
      'resetImporter',
      'requestCredentialsDetails',
      'requestExportProgressStatus',
      'requestPlaintextPassword',
      'showAddShortcutDialog',
      'showExportedFileInShell',
      'sharePassword',
      'startBulkPasswordCheck',
      'switchBiometricAuthBeforeFillingState',
      'undoRemoveSavedPasswordOrException',
      'unmuteInsecureCredential',
    ]);

    // Set these to have non-empty data.
    this.data = {
      blockedSites: [],
      checkStatus: makePasswordCheckStatus({}),
      credentialWithReusedPassword: [],
      familyFetchResults: makeFamilyFetchResults(),
      groups: [],
      insecureCredentials: [],
      isOptedInAccountStorage: false,
      isAccountStorageDefault: false,
      passwords: [],
    };

    // Holds listeners so they can be called when needed.
    this.listeners = {
      accountStorageOptInStateListener: null,
      blockedSitesListChangedListener: null,
      insecureCredentialsListener: null,
      passwordCheckStatusListener: null,
      passwordsFileExportProgressListener: null,
      passwordManagerAuthTimeoutListener: null,
      savedPasswordListChangedListener: null,
    };
  }

  addSavedPasswordListChangedListener(listener: CredentialsChangedListener) {
    this.listeners.savedPasswordListChangedListener = listener;
  }

  removeSavedPasswordListChangedListener(_listener:
                                             CredentialsChangedListener) {
    this.listeners.savedPasswordListChangedListener = null;
  }

  addBlockedSitesListChangedListener(listener:
                                         BlockedSitesListChangedListener) {
    this.listeners.blockedSitesListChangedListener = listener;
  }

  removeBlockedSitesListChangedListener(_listener:
                                            BlockedSitesListChangedListener) {
    this.listeners.blockedSitesListChangedListener = null;
  }

  addPasswordCheckStatusListener(listener: PasswordCheckStatusChangedListener) {
    this.listeners.passwordCheckStatusListener = listener;
  }

  removePasswordCheckStatusListener(_listener:
                                        PasswordCheckStatusChangedListener) {
    this.listeners.passwordCheckStatusListener = null;
  }

  addInsecureCredentialsListener(listener: CredentialsChangedListener) {
    this.listeners.insecureCredentialsListener = listener;
  }

  removeInsecureCredentialsListener(_listener: CredentialsChangedListener) {
    this.listeners.insecureCredentialsListener = null;
  }

  getSavedPasswordList(): Promise<chrome.passwordsPrivate.PasswordUiEntry[]> {
    this.methodCalled('getSavedPasswordList');
    return Promise.resolve(this.data.passwords.slice());
  }

  getCredentialGroups(): Promise<chrome.passwordsPrivate.CredentialGroup[]> {
    this.methodCalled('getCredentialGroups');
    return Promise.resolve(this.data.groups.slice());
  }

  getBlockedSitesList(): Promise<BlockedSite[]> {
    this.methodCalled('getBlockedSitesList');
    return Promise.resolve(this.data.blockedSites.slice());
  }

  getPasswordCheckStatus() {
    this.methodCalled('getPasswordCheckStatus');
    return Promise.resolve(this.data.checkStatus);
  }

  getInsecureCredentials() {
    this.methodCalled('getInsecureCredentials');
    return Promise.resolve(this.data.insecureCredentials.slice());
  }

  getCredentialsWithReusedPassword() {
    this.methodCalled('getCredentialsWithReusedPassword');
    return Promise.resolve(this.data.credentialWithReusedPassword.slice());
  }

  startBulkPasswordCheck() {
    this.methodCalled('startBulkPasswordCheck');
    if (this.data.checkStatus.state ===
        chrome.passwordsPrivate.PasswordCheckState.NO_PASSWORDS) {
      return Promise.reject(new Error('error'));
    }
    return Promise.resolve();
  }

  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction) {
    this.methodCalled('recordPasswordCheckInteraction', interaction);
  }

  recordPasswordViewInteraction(interaction: PasswordViewPageInteractions) {
    this.methodCalled('recordPasswordViewInteraction', interaction);
  }

  muteInsecureCredential(insecureCredential:
                             chrome.passwordsPrivate.PasswordUiEntry) {
    this.methodCalled('muteInsecureCredential', insecureCredential);
  }

  unmuteInsecureCredential(insecureCredential:
                               chrome.passwordsPrivate.PasswordUiEntry) {
    this.methodCalled('unmuteInsecureCredential', insecureCredential);
  }

  showAddShortcutDialog() {
    this.methodCalled('showAddShortcutDialog');
  }

  requestCredentialsDetails(ids: number[]) {
    this.methodCalled('requestCredentialsDetails', ids);
    if (!this.requestCredentialsDetailsResponse_) {
      return Promise.reject(new Error('Could not obtain credential details'));
    }
    return Promise.resolve(this.requestCredentialsDetailsResponse_);
  }

  setRequestCredentialsDetailsResponse(
      credentials: chrome.passwordsPrivate.PasswordUiEntry[]) {
    this.requestCredentialsDetailsResponse_ = credentials;
  }

  requestPlaintextPassword(
      id: number, reason: chrome.passwordsPrivate.PlaintextReason) {
    this.methodCalled('requestPlaintextPassword', {id, reason});
    return Promise.resolve('plainTextPassword');
  }

  addPassword(options: chrome.passwordsPrivate.AddPasswordOptions) {
    this.methodCalled('addPassword', options);
    return Promise.resolve();
  }

  changeCredential(credential: chrome.passwordsPrivate.PasswordUiEntry) {
    this.methodCalled('changeCredential', credential);
    return Promise.resolve();
  }

  removeCredential(
      id: number, fromStores: chrome.passwordsPrivate.PasswordStoreSet) {
    this.methodCalled('removeCredential', {id, fromStores});
  }

  removeBlockedSite(id: number) {
    this.methodCalled('removeBlockedSite', id);
  }

  requestExportProgressStatus() {
    this.methodCalled('requestExportProgressStatus');
    return Promise.resolve(
        chrome.passwordsPrivate.ExportProgressStatus.NOT_STARTED);
  }

  exportPasswords() {
    this.methodCalled('exportPasswords');
    return Promise.resolve();
  }

  addPasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener) {
    this.listeners.passwordsFileExportProgressListener = listener;
  }

  removePasswordsFileExportProgressListener(
      _listener: PasswordsFileExportProgressListener) {
    this.listeners.passwordsFileExportProgressListener = null;
  }

  cancelExportPasswords() {
    this.methodCalled('cancelExportPasswords');
  }

  switchBiometricAuthBeforeFillingState() {
    this.methodCalled('switchBiometricAuthBeforeFillingState');
  }

  undoRemoveSavedPasswordOrException() {
    this.methodCalled('undoRemoveSavedPasswordOrException');
  }

  showExportedFileInShell() {
    this.methodCalled('showExportedFileInShell');
  }

  getUrlCollection(url: string) {
    this.methodCalled('getUrlCollection', url);
    if (url.includes('www')) {
      return Promise.resolve({
        signonRealm: `https://${url}/login`,
        shown: url,
        link: `https://${url}/login`,
      });
    } else {
      return Promise.reject();
    }
  }

  addPasswordManagerAuthTimeoutListener(
      listener: PasswordManagerAuthTimeoutListener) {
    this.listeners.passwordManagerAuthTimeoutListener = listener;
  }

  removePasswordManagerAuthTimeoutListener(
      _listener: PasswordManagerAuthTimeoutListener) {
    this.listeners.passwordManagerAuthTimeoutListener = null;
  }

  extendAuthValidity() {
    this.methodCalled('extendAuthValidity');
  }

  addAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener) {
    this.listeners.accountStorageOptInStateListener = listener;
  }

  removeAccountStorageOptInStateListener(
      _listener: AccountStorageOptInStateChangedListener) {
    this.listeners.accountStorageOptInStateListener = null;
  }

  fetchFamilyMembers() {
    this.methodCalled('fetchFamilyMembers');
    return Promise.resolve(this.data.familyFetchResults);
  }

  sharePassword(
      id: number, recipients: chrome.passwordsPrivate.RecipientInfo[]) {
    this.methodCalled('sharePassword', id, recipients);
  }

  /**
   * Sets the value to be returned by importPasswords.
   */
  setImportResults(results: chrome.passwordsPrivate.ImportResults) {
    this.importResults_ = results;
  }

  importPasswords(toStore: chrome.passwordsPrivate.PasswordStoreSet) {
    this.methodCalled('importPasswords', toStore);
    return Promise.resolve(this.importResults_);
  }

  continueImport(selectedIds: number[]) {
    this.methodCalled('continueImport', selectedIds);
    return Promise.resolve(this.importResults_);
  }

  resetImporter(deleteFile: boolean) {
    this.methodCalled('resetImporter', deleteFile);
    return Promise.resolve();
  }

  isOptedInForAccountStorage() {
    this.methodCalled('isOptedInForAccountStorage');
    return Promise.resolve(this.data.isOptedInAccountStorage);
  }

  optInForAccountStorage(optIn: boolean) {
    this.methodCalled('optInForAccountStorage');
    this.data.isOptedInAccountStorage = optIn;
  }

  isAccountStoreDefault() {
    this.methodCalled('isAccountStoreDefault');
    return Promise.resolve(this.data.isAccountStorageDefault);
  }

  movePasswordsToAccount(ids: number[]) {
    this.methodCalled('movePasswordsToAccount', ids);
  }

  dismissSafetyHubPasswordMenuNotification() {
    this.methodCalled('dismissSafetyHubPasswordMenuNotification');
  }
}
