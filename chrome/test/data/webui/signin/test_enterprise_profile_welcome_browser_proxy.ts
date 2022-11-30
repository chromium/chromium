// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EnterpriseProfileInfo, EnterpriseProfileWelcomeBrowserProxy} from 'chrome://enterprise-profile-welcome/enterprise_profile_welcome_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestEnterpriseProfileWelcomeBrowserProxy extends TestBrowserProxy
    implements EnterpriseProfileWelcomeBrowserProxy {
  private enterpriseProfileInfo_: EnterpriseProfileInfo;

  constructor(info: EnterpriseProfileInfo) {
    super([
      'initialized',
      'proceed',
      'cancel',
    ]);

    this.enterpriseProfileInfo_ = info;
  }

  setEnterpriseProfileInfo(info: EnterpriseProfileInfo) {
    this.enterpriseProfileInfo_ = info;
  }

  initialized() {
    this.methodCalled('initialized');
    return Promise.resolve(this.enterpriseProfileInfo_);
  }

  initializedWithSize(height: number) {
    this.methodCalled('initializedWithSize', height);
  }

  proceed() {
    this.methodCalled('proceed');
  }

  cancel() {
    this.methodCalled('cancel');
  }
}
