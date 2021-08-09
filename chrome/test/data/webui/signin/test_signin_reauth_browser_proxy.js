// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

/** @implements {SigninReauthBrowserProxy} */
export class TestSigninReauthBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['initialize', 'confirm', 'cancel']);
  }

  /** @override */
  initialize() {
    this.methodCalled('initialize');
  }

  /** @override */
  confirm() {
    this.methodCalled('confirm');
  }

  /** @override */
  cancel() {
    this.methodCalled('cancel');
  }
}
