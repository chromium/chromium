// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DataManagementSurvey, PasswordCount, SavedInfoHandlerProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSavedInfoHandlerProxy extends TestBrowserProxy implements
    SavedInfoHandlerProxy {
  private passwordCount_: PasswordCount = {passwordCount: 0, passkeyCount: 0};
  private loyaltyCardsCount_: number|undefined = undefined;

  constructor() {
    super([
      'getPasswordCount',
      'getLoyaltyCardsCount',
      'requestDataManagementSurvey',
    ]);
  }

  setPasswordCount(passwordCount: PasswordCount) {
    this.passwordCount_ = passwordCount;
  }

  setLoyaltyCardsCount(loyaltyCardsCount: number|undefined) {
    this.loyaltyCardsCount_ = loyaltyCardsCount;
  }

  getPasswordCount(): Promise<PasswordCount> {
    this.methodCalled('getPasswordCount');
    return Promise.resolve(this.passwordCount_);
  }

  getLoyaltyCardsCount(): Promise<number|undefined> {
    this.methodCalled('getLoyaltyCardsCount');
    return Promise.resolve(this.loyaltyCardsCount_);
  }

  requestDataManagementSurvey(
      survey: DataManagementSurvey, isFromHomePage: boolean) {
    this.methodCalled('requestDataManagementSurvey', [survey, isFromHomePage]);
  }
}
