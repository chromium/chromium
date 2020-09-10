// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetricsBrowserProxy} from 'chrome://settings/settings.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {MetricsBrowserProxy} */
export class TestMetricsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'recordAction',
      'recordSafetyCheckInteractionHistogram',
      'recordSettingsPageHistogram',
      'recordSafeBrowsingInteractionHistogram',
    ]);
  }

  /** @override */
  recordAction(action) {
    this.methodCalled('recordAction', action);
  }

  /** @override */
  recordSafetyCheckInteractionHistogram(interaction) {
    this.methodCalled('recordSafetyCheckInteractionHistogram', interaction);
  }

  /** @override */
  recordSettingsPageHistogram(interaction) {
    this.methodCalled('recordSettingsPageHistogram', interaction);
  }

  /** @override */
  recordSafeBrowsingInteractionHistogram(interaction) {
    this.methodCalled('recordSafeBrowsingInteractionHistogram', interaction);
  }
}
