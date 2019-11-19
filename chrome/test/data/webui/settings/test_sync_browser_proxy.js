// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.SyncBrowserProxy} */
class TestSyncBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
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
    ]);

    /** @private {number} */
    this.impressionCount_ = 0;

    /** @type {!settings.PageStatus} */
    this.encryptionResponse = settings.PageStatus.CONFIGURE;
  }

  /** @override */
  getSyncStatus() {
    this.methodCalled('getSyncStatus');
    return Promise.resolve({signedIn: true, signedInUsername: 'fakeUsername'});
  }

  /** @override */
  getStoredAccounts() {
    this.methodCalled('getStoredAccounts');
    return Promise.resolve([]);
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
    return Promise.resolve(settings.PageStatus.CONFIGURE);
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
}
