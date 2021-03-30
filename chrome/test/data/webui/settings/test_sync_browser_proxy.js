// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {PageStatus, StoredAccount, SyncBrowserProxy, SyncStatus} from 'chrome://settings/settings.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';
// clang-format on

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
      'setEncryptionPassphrase',
      'setDecryptionPassphrase',
      'signOut',
      'pauseSync',
      'sendSyncPrefsChanged',
      'startSignIn',
      'startSyncingWithEmail',
    ];

    if (isChromeOS) {
      methodNames.push('turnOnSync', 'turnOffSync');
    }

    super(methodNames);

    /** @private {number} */
    this.impressionCount_ = 0;

    // Settable fake data.
    /** @type {boolean} */
    this.encryptionPassphraseSuccess = false;
    /** @type {boolean} */
    this.decryptionPassphraseSuccess = false;
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
  setEncryptionPassphrase(passphrase) {
    this.methodCalled('setEncryptionPassphrase', passphrase);
    return Promise.resolve(this.encryptionPassphraseSuccess);
  }

  /** @override */
  setDecryptionPassphrase(passphrase) {
    this.methodCalled('setDecryptionPassphrase', passphrase);
    return Promise.resolve(this.decryptionPassphraseSuccess);
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

if (isChromeOS) {
  /** @override */
  TestSyncBrowserProxy.prototype.turnOnSync = function() {
    this.methodCalled('turnOnSync');
  };

  /** @override */
  TestSyncBrowserProxy.prototype.turnOffSync = function() {
    this.methodCalled('turnOffSync');
  };
}
