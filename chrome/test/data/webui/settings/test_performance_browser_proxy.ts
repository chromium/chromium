// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PerformanceBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPerformanceBrowserProxy extends TestBrowserProxy implements
    PerformanceBrowserProxy {
  private validationResult_: boolean = true;

  constructor() {
    super([
      'getDeviceHasBattery',
      'openBatterySaverFeedbackDialog',
      'openHighEfficiencyFeedbackDialog',
      'validateTabDiscardExceptionRule',
    ]);
  }

  getDeviceHasBattery() {
    this.methodCalled('getDeviceHasBattery');
    return Promise.resolve(false);
  }

  openBatterySaverFeedbackDialog() {
    this.methodCalled('openBatterySaverFeedbackDialog');
  }

  openHighEfficiencyFeedbackDialog() {
    this.methodCalled('openHighEfficiencyFeedbackDialog');
  }

  setValidationResult(result: boolean) {
    this.validationResult_ = result;
  }

  validateTabDiscardExceptionRule(rule: string) {
    this.methodCalled('validateTabDiscardExceptionRule', rule);
    return Promise.resolve(this.validationResult_);
  }
}