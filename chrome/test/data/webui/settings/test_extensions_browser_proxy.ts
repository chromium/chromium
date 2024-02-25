// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SafetyCheckExtensionsBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestExtensionBrowserProxy extends TestBrowserProxy implements
    SafetyCheckExtensionsBrowserProxy {
  constructor() {
    super([
      'getNumberOfExtensionsThatNeedReview',
    ]);
  }

  getNumberOfExtensionsThatNeedReview() {
    this.methodCalled('getNumberOfExtensionsThatNeedReview');
    return Promise.resolve(10);
  }
}
