// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerRemote} from 'chrome://history/history.js';
import {TestBrowserProxy as BaseTestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

export class TestBrowserProxy extends BaseTestBrowserProxy {
  constructor() {
    super([]);
    this.handler = TestBrowserProxy.fromClass(PageHandlerRemote);
    this.callbackRouter = new PageCallbackRouter();
  }
}

export class TestMetricsProxy extends BaseTestBrowserProxy {
  constructor() {
    super(['recordToggledVisibility']);
  }

  recordToggledVisibility(visible) {
    this.methodCalled('recordToggledVisibility', visible);
  }
}
