// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PasswordManagerProxy. */

import {BlockedSite, BlockedSitesListChangedListener, CredentialsChangedListener, PasswordCheckInteraction, PasswordCheckStatusChangedListener, PasswordManagerProxy, PasswordsFileExportProgressListener} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {makePasswordCheckStatus} from './test_util.js';

/**
 * Test implementation
 */
export class TestPasswordManagerProxy extends TestBrowserProxy implements
    PasswordManagerProxy {
  data: {
    passwords: chrome.passwordsPrivate.PasswordUiEntry[],
    groups: chrome.passwordsPrivate.CredentialGroup[],
    blockedSites: BlockedSite[],
    checkStatus: chrome.passwordsPrivate.PasswordCheckStatus,
    insecureCredentials: chrome.passwordsPrivate.PasswordUiEntry[],
  };

  listeners: {
    blockedSitesListChangedListener: BlockedSitesListChangedListener|null,
    savedPasswordListChangedListener: CredentialsChangedListener|null,
    passwordCheckStatusListener: PasswordCheckStatusChangedListener|null,
    insecureCredentialsListener: CredentialsChangedListener|null,
    passwordsFileExportProgressListener: PasswordsFileExportProgressListener|
    null,
  };

  private requestCredentialsDetailsResponse_:
      chrome.passwordsPrivate.PasswordUiEntry[]|null = null;

  constructor() {
    super([
      'cancelExportPasswords',
      'exportPasswords',
      'getBlockedSitesList',
      'getCredentialGroups',
      'getInsecureCredentials',
      'getPasswordCheckStatus',
      'getSavedPasswordList',
      'recordPasswordCheckInteraction',
      'removeBlockedSite',
      'requestCredentialsDetails',
      'requestExportProgressStatus',
      'requestPlaintextPassword',
      'startBulkPasswordCheck',
    ]);

    // Set these to have non-empty data.
    this.data = {
      passwords: [],
      groups: [],
      blockedSites: [],
      checkStatus: makePasswordCheckStatus({}),
      insecureCredentials: [],
    };

    // Holds listeners so they can be called when needed.
    this.listeners = {
      passwordCheckStatusListener: null,
      blockedSitesListChangedListener: null,
      savedPasswordListChangedListener: null,
      insecureCredentialsListener: null,
      passwordsFileExportProgressListener: null,
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
}
