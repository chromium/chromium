// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PasswordManagerProxy. */

// clang-format off
import {AccountStorageOptInStateChangedListener, CredentialsChangedListener, PasswordCheckInteraction, PasswordCheckReferrer, PasswordCheckStatusChangedListener, PasswordExceptionListChangedListener, PasswordManagerProxy, PasswordsFileExportProgressListener, SavedPasswordListChangedListener} from 'chrome://settings/settings.js';
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
    leakedCredentials: chrome.passwordsPrivate.InsecureCredential[],
    weakCredentials: chrome.passwordsPrivate.InsecureCredential[],
    checkStatus: chrome.passwordsPrivate.PasswordCheckStatus,
  };

  lastCallback: {
    addPasswordCheckStatusListener: PasswordCheckStatusChangedListener|null,
    addSavedPasswordListChangedListener: SavedPasswordListChangedListener|null,
    addExceptionListChangedListener: PasswordExceptionListChangedListener|null,
    addCompromisedCredentialsListener: CredentialsChangedListener|null,
    addWeakCredentialsListener: CredentialsChangedListener|null,
    addAccountStorageOptInStateListener:
        AccountStorageOptInStateChangedListener|null,
    addPasswordsFileExportProgressListener: PasswordsFileExportProgressListener|
    null,
  };

  private plaintextPassword_: string = '';
  private isOptedInForAccountStorage_: boolean = false;
  private isAccountStoreDefault_: boolean = false;
  private getUrlCollectionResponse_: chrome.passwordsPrivate.UrlCollection|
      null = null;

  constructor() {
    super([
      'requestPlaintextPassword',
      'startBulkPasswordCheck',
      'stopBulkPasswordCheck',
      'getCompromisedCredentials',
      'getWeakCredentials',
      'getPasswordCheckStatus',
      'getPlaintextInsecurePassword',
      'changeInsecureCredential',
      'removeInsecureCredential',
      'recordPasswordCheckInteraction',
      'recordPasswordCheckReferrer',
      'isOptedInForAccountStorage',
      'removeSavedPassword',
      'removeSavedPasswords',
      'movePasswordsToAccount',
      'removeException',
      'removeExceptions',
      'changeSavedPassword',
      'isAccountStoreDefault',
      'getUrlCollection',
      'addPassword',
      'muteInsecureCredential',
      'unmuteInsecureCredential',
      'recordChangePasswordFlowStarted',
    ]);

    /** @private {!PasswordManagerExpectations} */
    this.actual_ = new PasswordManagerExpectations();

    // Set these to have non-empty data.
    this.data = {
      passwords: [],
      exceptions: [],
      leakedCredentials: [],
      weakCredentials: [],
      checkStatus: makePasswordCheckStatus(),
    };

    // Holds the last callbacks so they can be called when needed/
    this.lastCallback = {
      addPasswordCheckStatusListener: null,
      addSavedPasswordListChangedListener: null,
      addExceptionListChangedListener: null,
      addCompromisedCredentialsListener: null,
      addWeakCredentialsListener: null,
      addAccountStorageOptInStateListener: null,
      addPasswordsFileExportProgressListener: null,
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

  getSavedPasswordList(callback: SavedPasswordListChangedListener) {
    this.actual_.requested.passwords++;
    callback(this.data.passwords);
  }

  recordPasswordsPageAccessInSettings() {}

  removeSavedPassword(id: number) {
    this.actual_.removed.passwords++;
    this.methodCalled('removeSavedPassword', id);
  }

  movePasswordsToAccount(ids: Array<number>) {
    this.methodCalled('movePasswordsToAccount', ids);
  }

  removeSavedPasswords(ids: Array<number>) {
    this.actual_.removed.passwords += ids.length;
    this.methodCalled('removeSavedPasswords', ids);
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

  getExceptionList(callback: PasswordExceptionListChangedListener) {
    this.actual_.requested.exceptions++;
    callback(this.data.exceptions);
  }

  removeException(id: number) {
    this.actual_.removed.exceptions++;
    this.methodCalled('removeException', id);
  }

  removeExceptions(ids: Array<number>) {
    this.actual_.removed.exceptions += ids.length;
    this.methodCalled('removeExceptions', ids);
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
    return Promise.resolve();
  }

  stopBulkPasswordCheck() {
    this.methodCalled('stopBulkPasswordCheck');
  }

  getCompromisedCredentials() {
    this.methodCalled('getCompromisedCredentials');
    return Promise.resolve(this.data.leakedCredentials);
  }

  getWeakCredentials() {
    this.methodCalled('getWeakCredentials');
    return Promise.resolve(this.data.weakCredentials);
  }

  getPasswordCheckStatus() {
    this.methodCalled('getPasswordCheckStatus');
    return Promise.resolve(this.data.checkStatus);
  }

  addCompromisedCredentialsListener(listener: CredentialsChangedListener) {
    this.lastCallback.addCompromisedCredentialsListener = listener;
  }

  removeCompromisedCredentialsListener(_listener: CredentialsChangedListener) {}

  addWeakCredentialsListener(listener: CredentialsChangedListener) {
    this.lastCallback.addWeakCredentialsListener = listener;
  }

  removeWeakCredentialsListener(_listener: CredentialsChangedListener) {}

  addPasswordCheckStatusListener(listener: PasswordCheckStatusChangedListener) {
    this.lastCallback.addPasswordCheckStatusListener = listener;
  }

  removePasswordCheckStatusListener(_listener:
                                        PasswordCheckStatusChangedListener) {}

  getPlaintextInsecurePassword(
      credential: chrome.passwordsPrivate.InsecureCredential,
      reason: chrome.passwordsPrivate.PlaintextReason) {
    this.methodCalled('getPlaintextInsecurePassword', {credential, reason});
    if (!this.plaintextPassword_) {
      return Promise.reject(new Error('Could not obtain plaintext password'));
    }

    const newCredential =
        /** @type {chrome.passwordsPrivate.InsecureCredential} */ (
            Object.assign({}, credential));
    newCredential.password = this.plaintextPassword_;
    return Promise.resolve(newCredential);
  }

  changeInsecureCredential(
      credential: chrome.passwordsPrivate.InsecureCredential,
      newPassword: string) {
    this.methodCalled('changeInsecureCredential', {credential, newPassword});
    return Promise.resolve();
  }

  removeInsecureCredential(insecureCredential:
                               chrome.passwordsPrivate.InsecureCredential) {
    this.methodCalled('removeInsecureCredential', insecureCredential);
  }

  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction) {
    this.methodCalled('recordPasswordCheckInteraction', interaction);
  }

  recordPasswordCheckReferrer(referrer: PasswordCheckReferrer) {
    this.methodCalled('recordPasswordCheckReferrer', referrer);
  }

  changeSavedPassword(
      ids: Array<number>,
      params: chrome.passwordsPrivate.ChangeSavedPasswordParams) {
    this.methodCalled('changeSavedPassword', {ids, params});
    return Promise.resolve();
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
                             chrome.passwordsPrivate.InsecureCredential) {
    this.methodCalled('muteInsecureCredential', insecureCredential);
  }

  unmuteInsecureCredential(insecureCredential:
                               chrome.passwordsPrivate.InsecureCredential) {
    this.methodCalled('unmuteInsecureCredential', insecureCredential);
  }

  recordChangePasswordFlowStarted(
      insecureCredential: chrome.passwordsPrivate.InsecureCredential,
      isManualFlow: boolean) {
    this.methodCalled(
        'recordChangePasswordFlowStarted', insecureCredential, isManualFlow);
  }

  cancelExportPasswords() {}

  exportPasswords(_callback: () => void) {}

  importPasswords() {}

  optInForAccountStorage(_optIn: boolean) {}

  removePasswordsFileExportProgressListener(
      _listener: PasswordsFileExportProgressListener) {}

  requestExportProgressStatus(
      _callback:
          (status: chrome.passwordsPrivate.ExportProgressStatus) => void) {}

  undoRemoveSavedPasswordOrException() {}
}
