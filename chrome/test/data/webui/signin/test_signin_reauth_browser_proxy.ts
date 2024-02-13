// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SigninReauthBrowserProxy} from 'chrome://signin-reauth/signin_reauth_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSigninReauthBrowserProxy extends TestBrowserProxy implements
    SigninReauthBrowserProxy {
  constructor() {
    super(['initialize', 'confirm', 'cancel']);
  }

  initialize() {
    this.methodCalled('initialize');
  }

  confirm() {
    this.methodCalled('confirm');
  }

  cancel() {
    this.methodCalled('cancel');
  }
}
