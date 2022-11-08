// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export const DEVICE_METRICS_CONSENT_PREF_NAME = 'cros.metrics.reportingEnabled';

/** @implements {MetricsConsentBrowserProxy} */
export class TestMetricsConsentBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getMetricsConsentState',
      'updateMetricsConsent',
    ]);

    /** @type {MetricsConsentState} */
    this.state_ = {
      prefName: DEVICE_METRICS_CONSENT_PREF_NAME,
      isConfigurable: false,
    };
  }

  /** @override */
  getMetricsConsentState() {
    this.methodCalled('getMetricsConsentState');
    return Promise.resolve(this.state_);
  }

  /** @override */
  updateMetricsConsent(consent) {
    this.methodCalled('updateMetricsConsent');
    return Promise.resolve(consent);
  }

  /**
   * @param {String} prefName
   * @param {Boolean} isConfigurable
   */
  setMetricsConsentState(prefName, isConfigurable) {
    this.state_.prefName = prefName;
    this.state_.isConfigurable = isConfigurable;
  }
}