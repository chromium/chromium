// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DataManagementSurvey, PasswordCount, SavedInfoHandlerProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSavedInfoHandlerProxy extends TestBrowserProxy implements
    SavedInfoHandlerProxy {
  private passwordCount_: PasswordCount = {passwordCount: 0, passkeyCount: 0};

  constructor() {
    super([
      'getPasswordCount',
      'requestDataManagementSurvey',
    ]);
  }

  setPasswordCount(passwordCount: PasswordCount) {
    this.passwordCount_ = passwordCount;
  }

  getPasswordCount(): Promise<PasswordCount> {
    this.methodCalled('getPasswordCount');
    return Promise.resolve(this.passwordCount_);
  }

  requestDataManagementSurvey(
      survey: DataManagementSurvey, isFromHomePage: boolean) {
    this.methodCalled('requestDataManagementSurvey', [survey, isFromHomePage]);
  }
}
