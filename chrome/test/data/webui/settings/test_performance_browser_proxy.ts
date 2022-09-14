// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PerformanceBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPerformanceBrowserProxy extends TestBrowserProxy implements
    PerformanceBrowserProxy {
  constructor() {
    super([
      'openHighEfficiencyFeedbackDialog',
    ]);
  }

  openHighEfficiencyFeedbackDialog() {
    this.methodCalled('openHighEfficiencyFeedbackDialog');
  }
}