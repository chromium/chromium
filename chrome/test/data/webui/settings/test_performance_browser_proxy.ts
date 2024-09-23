// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PerformanceBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPerformanceBrowserProxy extends TestBrowserProxy implements
    PerformanceBrowserProxy {
  private currentSites_: string[] = [];
  private validationResults_: Record<string, boolean> = {};

  constructor() {
    super([
      'getCurrentOpenSites',
      'getDeviceHasBattery',
      'openFeedbackDialog',
      'validateTabDiscardExceptionRule',
    ]);
  }

  setCurrentOpenSites(currentSites: string[]) {
    this.currentSites_ = currentSites;
  }

  getCurrentOpenSites() {
    this.methodCalled('getCurrentOpenSites');
    return Promise.resolve(this.currentSites_);
  }

  getDeviceHasBattery() {
    this.methodCalled('getDeviceHasBattery');
    return Promise.resolve(false);
  }

  openFeedbackDialog(categoryTag: string) {
    this.methodCalled('openFeedbackDialog', categoryTag);
  }

  setValidationResults(results: Record<string, boolean>) {
    this.validationResults_ = results;
  }

  validateTabDiscardExceptionRule(rule: string) {
    this.methodCalled('validateTabDiscardExceptionRule', rule);
    return Promise.resolve(this.validationResults_[rule] ?? true);
  }
}