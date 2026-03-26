// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {MetricsReporting, PrivacyPageBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

export class TestPrivacyPageBrowserProxy extends TestBrowserProxy implements
    PrivacyPageBrowserProxy {
  metricsReporting: MetricsReporting;

  constructor() {
    super([
      'getMetricsReporting',
      'setMetricsReportingEnabled',
    ]);

    this.metricsReporting = {
      enabled: true,
      managed: true,
    };
  }

  getMetricsReporting() {
    this.methodCalled('getMetricsReporting');
    return Promise.resolve(this.metricsReporting);
  }

  setMetricsReportingEnabled(enabled: boolean) {
    this.methodCalled('setMetricsReportingEnabled', enabled);
  }
}
