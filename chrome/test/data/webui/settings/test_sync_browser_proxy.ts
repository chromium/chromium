// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {StoredAccount, SyncBrowserProxy, SyncPrefs, SyncStatus} from 'chrome://settings/settings.js';
import type {ChromeSigninUserChoiceInfo} from 'chrome://settings/settings.js';
import {PageStatus, SignedInState, StatusAction, ChromeSigninUserChoice} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// <if expr="not is_chromeos">
import type {ChromeSigninAccessPoint, UserSelectableType} from 'chrome://settings/settings.js';
// </if>

// clang-format on

export class TestSyncBrowserProxy extends TestBrowserProxy implements
    SyncBrowserProxy {
  private resolveGetSyncStatus_: Function|null = null;
  private syncStatus_: SyncStatus|null = {
    signedInState: SignedInState.SYNCING,
    signedInUsername: 'fakeUsername',
    statusAction: StatusAction.NO_ACTION,
  };

  // Settable fake data.
  encryptionPassphraseSuccess: boolean = false;
  decryptionPassphraseSuccess: boolean = false;
  storedAccounts: StoredAccount[] = [];
  profileAvatarURL: string = '';
  chromeSigninUserChoiceInfo: ChromeSigninUserChoiceInfo = {
    shouldShowSettings: false,
    choice: ChromeSigninUserChoice.NO_CHOICE,
    signedInEmail: '',
  };

  constructor() {
    // clang-format off
    super([
      'didNavigateAwayFromSyncPage',
      'didNavigateToSyncPage',
      'getStoredAccounts',
      'getProfileAvatar',
      'getSyncStatus',
      'setSyncDatatypes',
      'setEncryptionPassphrase',
      'setDecryptionPassphrase',
      'sendSyncPrefsChanged',
      'sendTrustedVaultBannerStateChanged',
      'startSyncingWithEmail',

      // <if expr="not is_chromeos">
      'pauseSync',
      'signOut',
      'startSignIn',
      'didNavigateToAccountSettingsPage',
      'setSyncDatatype',
      'recordSigninPendingOffered',
      // </if>

      // <if expr="is_chromeos">
      'turnOnSync',
      'turnOffSync',
      // </if>
      'setChromeSigninUserChoice',
      'getChromeSigninUserChoiceInfo',
    ]);
    // clang-format on
  }

  get testSyncStatus(): SyncStatus|null {
    return this.syncStatus_;
  }

  set testSyncStatus(syncStatus: SyncStatus|null) {
    this.syncStatus_ = syncStatus;
    if (this.syncStatus_ && this.resolveGetSyncStatus_) {
      this.resolveGetSyncStatus_(this.syncStatus_);
      this.resolveGetSyncStatus_ = null;
    }
  }

  getSyncStatus(): Promise<SyncStatus> {
    this.methodCalled('getSyncStatus');
    if (this.syncStatus_) {
      return Promise.resolve(this.syncStatus_);
    } else {
      return new Promise((resolve) => {
        this.resolveGetSyncStatus_ = resolve;
      });
    }
  }

  getStoredAccounts() {
    this.methodCalled('getStoredAccounts');
    return Promise.resolve(this.storedAccounts);
  }

  getProfileAvatar() {
    this.methodCalled('getProfileAvatar');
    return Promise.resolve(this.profileAvatarURL);
  }

  // <if expr="not is_chromeos">
  signOut(deleteProfile: boolean) {
    this.methodCalled('signOut', deleteProfile);
  }

  pauseSync() {
    this.methodCalled('pauseSync');
  }

  startSignIn(accessPoint: ChromeSigninAccessPoint) {
    this.methodCalled('startSignIn', accessPoint);
  }

  didNavigateToAccountSettingsPage() {
    this.methodCalled('didNavigateToAccountSettingsPage');
  }

  setSyncDatatype(pref: UserSelectableType, value: boolean) {
    this.methodCalled('setSyncDatatype', pref, value);
    return Promise.resolve(PageStatus.CONFIGURE);
  }

  recordSigninPendingOffered(): void {
    this.methodCalled('recordSigninPendingOffered');
  }
  // </if>

  startSyncingWithEmail(email: string, isDefaultPromoAccount: boolean) {
    this.methodCalled('startSyncingWithEmail', [email, isDefaultPromoAccount]);
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

  sendTrustedVaultBannerStateChanged() {
    this.methodCalled('sendTrustedVaultBannerStateChanged');
  }

  openActivityControlsUrl() {}

  startKeyRetrieval() {}

  showSyncPassphraseDialog() {}

  // <if expr="is_chromeos">
  attemptUserExit() {}

  turnOnSync() {
    this.methodCalled('turnOnSync');
  }

  turnOffSync() {
    this.methodCalled('turnOffSync');
  }
  // </if>

  setChromeSigninUserChoice(): void {
    this.methodCalled('setChromeSigninUserChoice');
  }

  // Prepares the return value for `getChromeSigninUserChoiceInfo()`.
  setGetUserChromeSigninUserChoiceInfoResponse(
      info: ChromeSigninUserChoiceInfo): void {
    this.chromeSigninUserChoiceInfo = info;
  }

  getChromeSigninUserChoiceInfo(): Promise<ChromeSigninUserChoiceInfo> {
    this.methodCalled('getChromeSigninUserChoiceInfo');
    return Promise.resolve(this.chromeSigninUserChoiceInfo);
  }
}
