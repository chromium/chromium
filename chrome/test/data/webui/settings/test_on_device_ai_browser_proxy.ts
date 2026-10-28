// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OnDeviceAiBrowserProxy, OnDeviceAiEnabled} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestOnDeviceAiBrowserProxy extends TestBrowserProxy implements
    OnDeviceAiBrowserProxy {
  private onDeviceAiEnabled_: OnDeviceAiEnabled = {
    enabled: true,
    allowedByPolicy: true,
  };

  constructor() {
    super([
      'getOnDeviceAiEnabled',
      'setOnDeviceAiEnabled',
      'openFeedbackDialog',
    ]);
  }

  getOnDeviceAiEnabled() {
    this.methodCalled('getOnDeviceAiEnabled');
    return Promise.resolve(this.onDeviceAiEnabled_);
  }

  setGetOnDeviceAiEnabledResponse(response: OnDeviceAiEnabled) {
    this.onDeviceAiEnabled_ = response;
  }

  setOnDeviceAiEnabled(enabled: boolean) {
    this.methodCalled('setOnDeviceAiEnabled', enabled);
  }

  openFeedbackDialog() {
    this.methodCalled('openFeedbackDialog');
  }
}
