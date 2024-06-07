// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrivacyGuideBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPrivacyGuideBrowserProxy extends TestBrowserProxy implements
    PrivacyGuideBrowserProxy {
  constructor() {
    super([
      'getPromoImpressionCount',
      'incrementPromoImpressionCount',
    ]);
  }

  getPromoImpressionCount() {
    this.methodCalled('getPromoImpressionCount');
    return 0;
  }

  incrementPromoImpressionCount() {
    this.methodCalled('incrementPromoImpressionCount');
  }
}
