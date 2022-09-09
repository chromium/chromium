// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {AndroidInfoBrowserProxy, AndroidSmsInfo} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/**
 * Test value for messages for web permissions origin.
 */
export const TEST_ANDROID_SMS_ORIGIN = 'http://foo.com';

export class TestAndroidInfoBrowserProxy extends TestBrowserProxy implements
    AndroidInfoBrowserProxy {
  androidSmsInfo:
      AndroidSmsInfo = {origin: TEST_ANDROID_SMS_ORIGIN, enabled: true};

  constructor() {
    super([
      'getAndroidSmsInfo',
    ]);
  }

  requestAndroidAppsInfo() {}

  getAndroidSmsInfo() {
    this.methodCalled('getAndroidSmsInfo');
    return Promise.resolve(this.androidSmsInfo);
  }
}
