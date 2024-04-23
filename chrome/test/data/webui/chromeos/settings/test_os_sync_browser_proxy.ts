// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeSigninUserChoice, ChromeSigninUserChoiceInfo, PageStatus, SignedInState, StatusAction, StoredAccount, SyncBrowserProxy, SyncPrefs, SyncStatus} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSyncBrowserProxy extends TestBrowserProxy implements
    SyncBrowserProxy {
  private impressionCount_: number = 0;
  private storedAccounts_: StoredAccount[] = [];
  private syncStatus_: SyncStatus = {
    signedInState: SignedInState.SYNCING,
    signedInUsername: 'fakeUsername',
    statusAction: StatusAction.NO_ACTION,
  };

  constructor() {
    super([
      'didNavigateAwayFromSyncPage',
      'didNavigateToSyncPage',
      'getPromoImpressionCount',
      'getStoredAccounts',
      'getSyncStatus',
      'incrementPromoImpressionCount',
      'pauseSync',
      'sendSyncPrefsChanged',
      'sendTrustedVaultBannerStateChanged',
      'setEncryptionPassphrase',
      'setDecryptionPassphrase',
      'setSyncDatatypes',
      'setSyncEncryption',
      'signOut',
      'startSignIn',
      'startSyncingWithEmail',
      'turnOnSync',
      'turnOffSync',
      'SetChromeSigninUserChoiceInfo',
      'GetChromeSigninUserChoice',
    ]);
  }

  getSyncStatus(): Promise<SyncStatus> {
    this.methodCalled('getSyncStatus');
    return Promise.resolve(this.syncStatus_);
  }

  getStoredAccounts(): Promise<StoredAccount[]> {
    this.methodCalled('getStoredAccounts');
    return Promise.resolve(this.storedAccounts_);
  }

  signOut(deleteProfile: boolean): void {
    this.methodCalled('signOut', deleteProfile);
  }

  pauseSync(): void {
    this.methodCalled('pauseSync');
  }

  startSignIn(): void {
    this.methodCalled('startSignIn');
  }

  startSyncingWithEmail(email: string, isDefaultPromoAccount: boolean): void {
    this.methodCalled('startSyncingWithEmail', [email, isDefaultPromoAccount]);
  }

  setImpressionCount(count: number): void {
    this.impressionCount_ = count;
  }

  turnOnSync(): void {
    this.methodCalled('turnOnSync');
  }

  turnOffSync(): void {
    this.methodCalled('turnOffSync');
  }

  getPromoImpressionCount(): number {
    this.methodCalled('getPromoImpressionCount');
    return this.impressionCount_;
  }

  incrementPromoImpressionCount(): void {
    this.methodCalled('incrementPromoImpressionCount');
  }

  didNavigateToSyncPage(): void {
    this.methodCalled('didNavigateToSyncPage');
  }

  didNavigateAwayFromSyncPage(abort: boolean): void {
    this.methodCalled('didNavigateAwayFromSyncPage', abort);
  }

  setSyncDatatypes(syncPrefs: SyncPrefs): Promise<PageStatus> {
    this.methodCalled('setSyncDatatypes', syncPrefs);
    return Promise.resolve(PageStatus.CONFIGURE);
  }

  setSyncEncryption(syncPrefs: SyncPrefs): Promise<PageStatus> {
    this.methodCalled('setSyncEncryption', syncPrefs);
    return Promise.resolve(PageStatus.CONFIGURE);
  }

  sendTrustedVaultBannerStateChanged(): void {
    this.methodCalled('sendTrustedVaultBannerStateChanged');
  }
  setEncryptionPassphrase(passphrase: string): Promise<boolean> {
    this.methodCalled('setEncryptionPassphrase', passphrase);
    return Promise.resolve(true);
  }

  setDecryptionPassphrase(passphrase: string): Promise<boolean> {
    this.methodCalled('setDecryptionPassphrase', passphrase);
    return Promise.resolve(true);
  }

  sendSyncPrefsChanged(): void {
    this.methodCalled('sendSyncPrefsChanged');
  }

  setChromeSigninUserChoice(): void {
    this.methodCalled('SetChromeSigninUserChoice');
  }

  getChromeSigninUserChoiceInfo(): Promise<ChromeSigninUserChoiceInfo> {
    this.methodCalled('SetChromeSigninUserChoiceInfo');
    return Promise.resolve({
      shouldShowSettings: false,
      choice: ChromeSigninUserChoice.NO_CHOICE,
      signedInEmail: '',
    });
  }

  attemptUserExit(): void {}

  openActivityControlsUrl(): void {}

  startKeyRetrieval(): void {}
}
