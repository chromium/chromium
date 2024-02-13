// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import type {NuxSetAsDefaultProxy} from 'chrome://welcome/set_as_default/nux_set_as_default_proxy.js';
import type {DefaultBrowserInfo} from 'chrome://welcome/shared/nux_types.js';

export class TestNuxSetAsDefaultProxy extends TestBrowserProxy implements
    NuxSetAsDefaultProxy {
  private defaultStatus_: DefaultBrowserInfo = {
    canBeDefault: true,
    isDefault: false,
    isDisabledByPolicy: false,
    isUnknownError: false,
  };

  constructor() {
    super([
      'requestDefaultBrowserState',
      'setAsDefault',
      'recordPageShown',
      'recordNavigatedAway',
      'recordSkip',
      'recordBeginSetDefault',
      'recordSuccessfullySetDefault',
      'recordNavigatedAwayThroughBrowserHistory',
    ]);
  }

  requestDefaultBrowserState() {
    this.methodCalled('requestDefaultBrowserState');
    return Promise.resolve(this.defaultStatus_);
  }

  setAsDefault() {
    this.methodCalled('setAsDefault');
  }

  setDefaultStatus(status: DefaultBrowserInfo) {
    this.defaultStatus_ = status;
  }

  recordPageShown() {
    this.methodCalled('recordPageShown');
  }

  recordNavigatedAway() {
    this.methodCalled('recordNavigatedAway');
  }

  recordSkip() {
    this.methodCalled('recordSkip');
  }

  recordBeginSetDefault() {
    this.methodCalled('recordBeginSetDefault');
  }

  recordSuccessfullySetDefault() {
    this.methodCalled('recordSuccessfullySetDefault');
  }

  recordNavigatedAwayThroughBrowserHistory() {
    this.methodCalled('recordNavigatedAwayThroughBrowserHistory');
  }
}
