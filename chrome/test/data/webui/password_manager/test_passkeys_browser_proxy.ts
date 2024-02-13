// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PasskeysBrowserProxy. */

import type {PasskeysBrowserProxy} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPasskeysBrowserProxy extends TestBrowserProxy implements
    PasskeysBrowserProxy {
  passkeysPresent: boolean = false;

  constructor() {
    super([
      'passkeysHasPasskeys',
      'passkeysManagePasskeys',
    ]);
  }

  hasPasskeys(): Promise<boolean> {
    this.methodCalled('passkeysHasPasskeys');
    return Promise.resolve(this.passkeysPresent);
  }

  managePasskeys(): void {
    this.methodCalled('passkeysManagePasskeys');
  }
}
