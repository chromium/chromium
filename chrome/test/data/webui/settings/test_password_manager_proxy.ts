// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PasswordManagerProxy. */

// clang-format off
import {AccountStorageOptInStateChangedListener, CredentialsChangedListener, HatsBrowserProxyImpl, PasswordCheckInteraction, PasswordCheckReferrer, PasswordCheckStatusChangedListener, PasswordExceptionListChangedListener, PasswordManagerProxy, PasswordsFileExportProgressListener, PasswordManagerAuthTimeoutListener, SavedPasswordListChangedListener, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {makePasswordCheckStatus} from './passwords_and_autofill_fake_data.js';

// clang-format on

export class PasswordManagerExpectations {
  requested: {
    passwords: number,
    exceptions: number,
    plaintextPassword: number,
    accountStorageOptInState: number,
  };

  removed: {
    passwords: number,
    exceptions: number,
  };

  listening: {
    passwords: number,
    exceptions: number,
    accountStorageOptInState: number,
  };

  constructor() {
    this.requested = {
      passwords: 0,
      exceptions: 0,
      plaintextPassword: 0,
      accountStorageOptInState: 0,
    };

    this.removed = {
      passwords: 0,
      exceptions: 0,
    };

    this.listening = {
      passwords: 0,
      exceptions: 0,
      accountStorageOptInState: 0,
    };
  }
}

/**
 * Test implementation
 */
export class TestPasswordManagerProxy extends TestBrowserProxy implements
    PasswordManagerProxy {
  private actual_: PasswordManagerExpectations;

  data: {
    passwords: chrome.passwordsPrivate.PasswordUiEntry[],
    exceptions: chrome.passwordsPrivate.ExceptionEntry[],
    insecureCredentials: chrome.passwordsPrivate.PasswordUiEntry[],
    checkStatus: chrome.passwordsPrivate.PasswordCheckStatus,
  };

  lastCallback: {
    addPasswordCheckStatusListener: PasswordCheckStatusChangedListener|null,
    addSavedPasswordListChangedListener: SavedPasswordListChangedListener|null,
    addExceptionListChangedListener: PasswordExceptionListChangedListener|null,
    addInsecureCredentialsListener: CredentialsChangedListener|null,
    addAccountStorageOptInStateListener:
        AccountStorageOptInStateChangedListener|null,
    addPasswordsFileExportProgressListener: PasswordsFileExportProgressListener|
    null,
    addPasswordManagerAuthTimeoutListener: PasswordManagerAuthTimeoutListener|
    null,
  };

  private plaintextPassword_: string = '';
  private importResults_: chrome.passwordsPrivate.ImportResults = {
    status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
    numberImported: 0,
    displayedEntries: [],
    fileName: '',
  };
  private isOptedInForAccountStorage_: boolean = false;
  private isAccountStoreDefault_: boolean = false;
  private getUrlCollectionResponse_: chrome.passwordsPrivate.UrlCollection|
      null = null;
  private changeSavedPasswordResponse_: number|null = null;
  private requestCredentialsDetailsResponse_:
      chrome.passwordsPrivate.PasswordUiEntry|null = null;

  constructor() {
    super([
      'addPassword',
      'cancelExportPasswords',
      'changeSavedPassword',
      'exportPasswords',
      'extendAuthValidity',
      'getInsecureCredentials',
      'getPasswordCheckStatus',
      'getUrlCollection',
      'getSavedPasswordList',
      'importPasswords',
      'continueImport',
      'resetImporter',
      'isAccountStoreDefault',
      'isOptedInForAccountStorage',
      'movePasswordsToAccount',
      'muteInsecureCredential',
      'recordChangePasswordFlowStarted',
      'recordPasswordCheckInteraction',
      'recordPasswordCheckReferrer',
      'removeException',
      'removeSavedPassword',
      'requestExportProgressStatus',
      'requestPlaintextPassword',
      'requestCredentialsDetails',
      'startBulkPasswordCheck',
      'stopBulkPasswordCheck',
      'switchBiometricAuthBeforeFillingState',
      'unmuteInsecureCredential',
    ]);

    /** @private {!PasswordManagerExpectations} */
    this.actual_ = new PasswordManagerExpectations();

    // Set these to have non-empty data.
    this.data = {
      passwords: [],
      exceptions: [],
      insecureCredentials: [],
      checkStatus: makePasswordCheckStatus(),
    };

    // Holds the last callbacks so they can be called when needed/
    this.lastCallback = {
      addPasswordCheckStatusListener: null,
      addSavedPasswordListChangedListener: null,
      addExceptionListChangedListener: null,
      addInsecureCredentialsListener: null,
      addAccountStorageOptInStateListener: null,
      addPasswordsFileExportProgressListener: null,
      addPasswordManagerAuthTimeoutListener: null,
    };
  }

  addSavedPasswordListChangedListener(listener:
                                          SavedPasswordListChangedListener) {
    this.actual_.listening.passwords++;
    this.lastCallback.addSavedPasswordListChangedListener = listener;
  }

  removeSavedPasswordListChangedListener(_listener:
                                             SavedPasswordListChangedListener) {
    this.actual_.listening.passwords--;
  }

  getSavedPasswordList() {
    this.methodCalled('getSavedPasswordList');
    this.actual_.requested.passwords++;
    return Promise.resolve(this.data.passwords);
  }

  recordPasswordsPageAccessInSettings() {}

  removeSavedPassword(
      id: number, fromStores: chrome.passwordsPrivate.PasswordStoreSet) {
    this.actual_.removed.passwords++;
    this.methodCalled('removeSavedPassword', {id, fromStores});
  }

  movePasswordsToAccount(ids: number[]) {
    this.methodCalled('movePasswordsToAccount', ids);
  }

  addExceptionListChangedListener(listener:
                                      PasswordExceptionListChangedListener) {
    this.actual_.listening.exceptions++;
    this.lastCallback.addExceptionListChangedListener = listener;
  }

  removeExceptionListChangedListener(_listener:
                                         PasswordExceptionListChangedListener) {
    this.actual_.listening.exceptions--;
  }

  getExceptionList() {
    this.actual_.requested.exceptions++;
    return Promise.resolve(this.data.exceptions);
  }

  removeException(id: number) {
    this.actual_.removed.exceptions++;
    this.methodCalled('removeException', id);
  }

  requestPlaintextPassword(
      id: number, reason: chrome.passwordsPrivate.PlaintextReason) {
    this.methodCalled('requestPlaintextPassword', {id, reason});
    if (!this.plaintextPassword_) {
      return Promise.reject(new Error('Could not obtain plaintext password'));
    }
    return Promise.resolve(this.plaintextPassword_);
  }

  setPlaintextPassword(plaintextPassword: string) {
    this.plaintextPassword_ = plaintextPassword;
  }

  requestCredentialsDetails(ids: number[]) {
    this.methodCalled('requestCredentialsDetails', {ids});
    if (!this.requestCredentialsDetailsResponse_) {
      return Promise.reject(new Error('Could not obtain credential details'));
    }
    return Promise.resolve([this.requestCredentialsDetailsResponse_]);
  }

  setRequestCredentialsDetailsResponse(
      credential: chrome.passwordsPrivate.PasswordUiEntry) {
    this.requestCredentialsDetailsResponse_ = credential;
  }

  // Sets the return value of isOptedInForAccountStorage calls and notifies
  // the last added listener.
  setIsOptedInForAccountStorageAndNotify(optIn: boolean) {
    this.isOptedInForAccountStorage_ = optIn;
    if (this.lastCallback.addAccountStorageOptInStateListener) {
      this.lastCallback.addAccountStorageOptInStateListener(
          this.isOptedInForAccountStorage_);
    }
  }

  addAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener) {
    this.actual_.listening.accountStorageOptInState++;
    this.lastCallback.addAccountStorageOptInStateListener = listener;
  }

  removeAccountStorageOptInStateListener(
      _listener: AccountStorageOptInStateChangedListener) {
    this.actual_.listening.accountStorageOptInState--;
  }

  isOptedInForAccountStorage() {
    this.methodCalled('isOptedInForAccountStorage');
    this.actual_.requested.accountStorageOptInState++;
    return Promise.resolve(this.isOptedInForAccountStorage_);
  }

  /**
   * Verifies expectations.
   */
  assertExpectations(expected: PasswordManagerExpectations) {
    const actual = this.actual_;

    assertEquals(expected.requested.passwords, actual.requested.passwords);
    assertEquals(expected.requested.exceptions, actual.requested.exceptions);
    assertEquals(
        expected.requested.plaintextPassword,
        actual.requested.plaintextPassword);
    assertEquals(
        expected.requested.accountStorageOptInState,
        actual.requested.accountStorageOptInState);

    assertEquals(expected.removed.passwords, actual.removed.passwords);
    assertEquals(expected.removed.exceptions, actual.removed.exceptions);

    assertEquals(expected.listening.passwords, actual.listening.passwords);
    assertEquals(expected.listening.exceptions, actual.listening.exceptions);
    assertEquals(
        expected.listening.accountStorageOptInState,
        actual.listening.accountStorageOptInState);
  }

  startBulkPasswordCheck() {
    this.methodCalled('startBulkPasswordCheck');
    if (this.data.checkStatus.state ===
        chrome.passwordsPrivate.PasswordCheckState.NO_PASSWORDS) {
      return Promise.reject(new Error('error'));
    }
    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.RAN_PASSWORD_CHECK);
    return Promise.resolve();
  }

  stopBulkPasswordCheck() {
    this.methodCalled('stopBulkPasswordCheck');
  }

  getInsecureCredentials() {
    this.methodCalled('getInsecureCredentials');
    return Promise.resolve(this.data.insecureCredentials.slice());
  }

  getPasswordCheckStatus() {
    this.methodCalled('getPasswordCheckStatus');
    return Promise.resolve(this.data.checkStatus);
  }

  addInsecureCredentialsListener(listener: CredentialsChangedListener) {
    this.lastCallback.addInsecureCredentialsListener = listener;
  }

  removeInsecureCredentialsListener(_listener: CredentialsChangedListener) {}

  addPasswordCheckStatusListener(listener: PasswordCheckStatusChangedListener) {
    this.lastCallback.addPasswordCheckStatusListener = listener;
  }

  removePasswordCheckStatusListener(_listener:
                                        PasswordCheckStatusChangedListener) {}

  addPasswordManagerAuthTimeoutListener(
      listener: PasswordManagerAuthTimeoutListener) {
    this.lastCallback.addPasswordManagerAuthTimeoutListener = listener;
  }

  removePasswordManagerAuthTimeoutListener(
      _listener: PasswordManagerAuthTimeoutListener) {}

  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction) {
    this.methodCalled('recordPasswordCheckInteraction', interaction);
  }

  recordPasswordCheckReferrer(referrer: PasswordCheckReferrer) {
    this.methodCalled('recordPasswordCheckReferrer', referrer);
  }

  setChangeSavedPasswordResponse(newId: number) {
    this.changeSavedPasswordResponse_ = newId;
  }

  changeSavedPassword(
      id: number, params: chrome.passwordsPrivate.ChangeSavedPasswordParams) {
    this.methodCalled('changeSavedPassword', {id, params});
    return !this.changeSavedPasswordResponse_ ?
        Promise.reject(new Error('Could not change password.')) :
        Promise.resolve(this.changeSavedPasswordResponse_);
  }

  /**
   * Sets the value to be returned by isAccountStoreDefault.
   */
  setIsAccountStoreDefault(isDefault: boolean) {
    this.isAccountStoreDefault_ = isDefault;
  }

  isAccountStoreDefault() {
    this.methodCalled('isAccountStoreDefault');
    return Promise.resolve(this.isAccountStoreDefault_);
  }

  optInForAccountStorage(_optIn: boolean) {}

  /**
   * Sets the value to be returned by getUrlCollection.
   */
  setGetUrlCollectionResponse(urlCollection:
                                  chrome.passwordsPrivate.UrlCollection|null) {
    this.getUrlCollectionResponse_ = urlCollection;
  }

  getUrlCollection(url: string) {
    this.methodCalled('getUrlCollection', url);
    return Promise.resolve(this.getUrlCollectionResponse_);
  }

  addPassword(options: chrome.passwordsPrivate.AddPasswordOptions) {
    this.methodCalled('addPassword', options);
    return Promise.resolve();
  }

  addPasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener) {
    this.lastCallback.addPasswordsFileExportProgressListener = listener;
  }

  muteInsecureCredential(insecureCredential:
                             chrome.passwordsPrivate.PasswordUiEntry) {
    this.methodCalled('muteInsecureCredential', insecureCredential);
  }

  unmuteInsecureCredential(insecureCredential:
                               chrome.passwordsPrivate.PasswordUiEntry) {
    this.methodCalled('unmuteInsecureCredential', insecureCredential);
  }

  recordChangePasswordFlowStarted(insecureCredential:
                                      chrome.passwordsPrivate.PasswordUiEntry) {
    this.methodCalled('recordChangePasswordFlowStarted', insecureCredential);
  }

  extendAuthValidity() {
    this.methodCalled('extendAuthValidity');
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

  /**
   * Sets the value to be returned by importPasswords.
   */
  setImportResults(results: chrome.passwordsPrivate.ImportResults) {
    this.importResults_ = results;
  }

  exportPasswords() {
    this.methodCalled('exportPasswords');
    return Promise.resolve();
  }

  cancelExportPasswords() {
    this.methodCalled('cancelExportPasswords');
  }

  removePasswordsFileExportProgressListener(
      _listener: PasswordsFileExportProgressListener) {}

  requestExportProgressStatus() {
    this.methodCalled('requestExportProgressStatus');
    return Promise.resolve(
        chrome.passwordsPrivate.ExportProgressStatus.NOT_STARTED);
  }

  switchBiometricAuthBeforeFillingState() {
    this.methodCalled('switchBiometricAuthBeforeFillingState');
  }

  undoRemoveSavedPasswordOrException() {}
}
