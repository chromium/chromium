// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import { PageStatus, StatusAction, StoredAccount, SyncBrowserProxy,SyncPrefs, SyncStatus} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

export class TestSyncBrowserProxy extends TestBrowserProxy implements
    SyncBrowserProxy {
  private impressionCount_: number = 0;

  // Settable fake data.
  private encryptionPassphraseSuccess: boolean = false;
  private decryptionPassphraseSuccess: boolean = false;
  storedAccounts: StoredAccount[] = [];
  syncStatus: SyncStatus = {
    signedIn: true,
    signedInUsername: 'fakeUsername',
    statusAction: StatusAction.NO_ACTION
  };

  constructor() {
    super([
      'didNavigateAwayFromSyncPage',
      'didNavigateToSyncPage',
      'getPromoImpressionCount',
      'getStoredAccounts',
      'getSyncStatus',
      'incrementPromoImpressionCount',
      'setSyncDatatypes',
      'setEncryptionPassphrase',
      'setDecryptionPassphrase',
      'sendSyncPrefsChanged',
      'sendOfferTrustedVaultOptInChanged',
      'startSyncingWithEmail',

      // <if expr="not chromeos">
      'pauseSync',
      'signOut',
      'startSignIn',
      // </if>

      // <if expr="chromeos">
      'turnOnSync',
      'turnOffSync',
      // </if>
    ]);
  }

  getSyncStatus() {
    this.methodCalled('getSyncStatus');
    return Promise.resolve(this.syncStatus);
  }

  getStoredAccounts() {
    this.methodCalled('getStoredAccounts');
    return Promise.resolve(this.storedAccounts);
  }

  // <if expr="not chromeos">
  signOut(deleteProfile: boolean) {
    this.methodCalled('signOut', deleteProfile);
  }

  pauseSync() {
    this.methodCalled('pauseSync');
  }

  startSignIn() {
    this.methodCalled('startSignIn');
  }
  // </if>

  startSyncingWithEmail(email: string, isDefaultPromoAccount: boolean) {
    this.methodCalled('startSyncingWithEmail', [email, isDefaultPromoAccount]);
  }

  setImpressionCount(count: number) {
    this.impressionCount_ = count;
  }

  getPromoImpressionCount() {
    this.methodCalled('getPromoImpressionCount');
    return this.impressionCount_;
  }

  incrementPromoImpressionCount() {
    this.methodCalled('incrementPromoImpressionCount');
  }

  didNavigateToSyncPage() {
    this.methodCalled('didNavigateToSyncPage');
  }

  didNavigateAwayFromSyncPage(didAbort: boolean) {
    this.methodCalled('didNavigateAwayFromSyncPage', didAbort);
  }

  setSyncDatatypes(syncPrefs: SyncPrefs) {
    this.methodCalled('setSyncDatatypes', syncPrefs);
    return Promise.resolve(PageStatus.CONFIGURE);
  }

  setEncryptionPassphrase(passphrase: string) {
    this.methodCalled('setEncryptionPassphrase', passphrase);
    return Promise.resolve(this.encryptionPassphraseSuccess);
  }

  setDecryptionPassphrase(passphrase: string) {
    this.methodCalled('setDecryptionPassphrase', passphrase);
    return Promise.resolve(this.decryptionPassphraseSuccess);
  }

  sendSyncPrefsChanged() {
    this.methodCalled('sendSyncPrefsChanged');
  }

  sendOfferTrustedVaultOptInChanged() {
    this.methodCalled('sendOfferTrustedVaultOptInChanged');
  }

  openActivityControlsUrl() {}

  startKeyRetrieval() {}

  // <if expr="chromeos">
  attemptUserExit() {}

  turnOnSync() {
    this.methodCalled('turnOnSync');
  }

  turnOffSync() {
    this.methodCalled('turnOffSync');
  }
  // </if>
}
