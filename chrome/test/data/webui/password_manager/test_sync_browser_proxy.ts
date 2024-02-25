// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of SyncBrowserProxy. */

import type {AccountInfo, SyncBrowserProxy, SyncInfo} from 'chrome://password-manager/password_manager.js';
import {TrustedVaultBannerState} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Test implementation
 */
export class TestSyncBrowserProxy extends TestBrowserProxy implements
    SyncBrowserProxy {
  trustedVaultState: TrustedVaultBannerState;
  accountInfo: AccountInfo;
  syncInfo: SyncInfo;

  constructor() {
    super([
      'getTrustedVaultBannerState',
      'getSyncInfo',
      'getAccountInfo',
    ]);

    this.trustedVaultState = TrustedVaultBannerState.NOT_SHOWN;
    this.accountInfo = {
      email: 'testemail@gmail.com',
    };
    this.syncInfo = {
      isEligibleForAccountStorage: false,
      isSyncingPasswords: false,
    };
  }

  getTrustedVaultBannerState() {
    this.methodCalled('getTrustedVaultBannerState');
    return Promise.resolve(this.trustedVaultState);
  }

  getSyncInfo() {
    this.methodCalled('getSyncInfo');
    return Promise.resolve(this.syncInfo);
  }

  getAccountInfo() {
    this.methodCalled('getAccountInfo');
    return Promise.resolve(this.accountInfo);
  }
}
