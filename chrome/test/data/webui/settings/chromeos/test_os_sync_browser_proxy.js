// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageStatus} from 'chrome://os-settings/chromeos/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {SyncBrowserProxy} */
export class TestSyncBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = [
      'didNavigateAwayFromSyncPage',
      'didNavigateToSyncPage',
      'getPromoImpressionCount',
      'getStoredAccounts',
      'getSyncStatus',
      'incrementPromoImpressionCount',
      'setSyncDatatypes',
      'setSyncEncryption',
      'signOut',
      'pauseSync',
      'sendSyncPrefsChanged',
      'startSignIn',
      'startSyncingWithEmail',
      'turnOnSync',
      'turnOffSync',
    ];

    super(methodNames);

    /** @private {number} */
    this.impressionCount_ = 0;

    // Settable fake data.
    /** @type {!PageStatus} */
    this.encryptionResponse = PageStatus.CONFIGURE;
    /** @type {!Array<!StoredAccount>} */
    this.storedAccounts = [];
    /** @type {!SyncStatus} */
    this.syncStatus = /** @type {!SyncStatus} */ (
        {signedIn: true, signedInUsername: 'fakeUsername'});
  }


  /** @override */
  getSyncStatus() {
    this.methodCalled('getSyncStatus');
    return Promise.resolve(this.syncStatus);
  }

  /** @override */
  getStoredAccounts() {
    this.methodCalled('getStoredAccounts');
    return Promise.resolve(this.storedAccounts);
  }

  /** @override */
  signOut(deleteProfile) {
    this.methodCalled('signOut', deleteProfile);
  }

  /** @override */
  pauseSync() {
    this.methodCalled('pauseSync');
  }

  /** @override */
  startSignIn() {
    this.methodCalled('startSignIn');
  }

  /** @override */
  startSyncingWithEmail(email, isDefaultPromoAccount) {
    this.methodCalled('startSyncingWithEmail', [email, isDefaultPromoAccount]);
  }

  setImpressionCount(count) {
    this.impressionCount_ = count;
  }

  /** @override */
  turnOnSync() {
    this.methodCalled('turnOnSync');
  }

  /** @override */
  turnOffSync() {
    this.methodCalled('turnOffSync');
  }

  /** @override */
  getPromoImpressionCount() {
    this.methodCalled('getPromoImpressionCount');
    return this.impressionCount_;
  }

  /** @override */
  incrementPromoImpressionCount() {
    this.methodCalled('incrementPromoImpressionCount');
  }

  /** @override */
  didNavigateToSyncPage() {
    this.methodCalled('didNavigateToSyncPage');
  }

  /** @override */
  didNavigateAwayFromSyncPage(abort) {
    this.methodCalled('didNavigateAwayFromSyncPage', abort);
  }

  /** @override */
  setSyncDatatypes(syncPrefs) {
    this.methodCalled('setSyncDatatypes', syncPrefs);
    return Promise.resolve(PageStatus.CONFIGURE);
  }

  /** @override */
  setSyncEncryption(syncPrefs) {
    this.methodCalled('setSyncEncryption', syncPrefs);
    return Promise.resolve(this.encryptionResponse);
  }

  /** @override */
  sendSyncPrefsChanged() {
    this.methodCalled('sendSyncPrefsChanged');
  }

  /** @override */
  attemptUserExit() {}

  /** @override */
  openActivityControlsUrl() {}

  /** @override */
  startKeyRetrieval() {}
}
