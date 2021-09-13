// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {SecureDnsMode, SecureDnsUiManagementMode} from 'chrome://settings/settings.js';

import {assertFalse} from '../chai_assert.js';
import {TestBrowserProxy} from '../test_browser_proxy.js';
// clang-format on

/** @implements {PrivacyPageBrowserProxy} */
export class TestPrivacyPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getMetricsReporting',
      'setMetricsReportingEnabled',
      'showManageSSLCertificates',
      'setBlockAutoplayEnabled',
      'getSecureDnsResolverList',
      'getSecureDnsSetting',
      'parseCustomDnsEntry',
      'probeCustomDnsTemplate',
      'recordUserDropdownInteraction',
    ]);

    /** @type {!MetricsReporting} */
    this.metricsReporting = {
      enabled: true,
      managed: true,
    };

    /**
     * @type {!SecureDnsSetting}
     * @private
     */
    this.secureDnsSetting = {
      mode: SecureDnsMode.AUTOMATIC,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    };

    /**
     * @type {!Array<!ResolverOption>}
     * @private
     */
    this.resolverList_ = [{name: 'Custom', value: 'custom', policy: ''}];

    /**
     * @type {!Array<string>}
     * @private
     */
    this.parsedEntry_ = [];

    /**
     * @type {!Object<string, boolean>}
     * @private
     */
    this.probeResults_;
  }

  /** @override */
  getMetricsReporting() {
    this.methodCalled('getMetricsReporting');
    return Promise.resolve(this.metricsReporting);
  }

  /** @override */
  setMetricsReportingEnabled(enabled) {
    this.methodCalled('setMetricsReportingEnabled', enabled);
  }

  /** @override */
  showManageSSLCertificates() {
    this.methodCalled('showManageSSLCertificates');
  }

  /** @override */
  setBlockAutoplayEnabled(enabled) {
    this.methodCalled('setBlockAutoplayEnabled', enabled);
  }

  /**
   * Sets the resolver list that will be returned when getSecureDnsResolverList
   * is called.
   * @param {!Array<!ResolverOption>} resolverList
   */
  setResolverList(resolverList) {
    this.resolverList_ = resolverList;
  }

  /** @override */
  getSecureDnsResolverList() {
    this.methodCalled('getSecureDnsResolverList');
    return Promise.resolve(this.resolverList_);
  }

  /** @override */
  getSecureDnsSetting() {
    this.methodCalled('getSecureDnsSetting');
    return Promise.resolve(this.secureDnsSetting);
  }

  /**
   * Sets the return value for the next parseCustomDnsEntry call.
   * @param {!Array<string>} parsedEntry
   */
  setParsedEntry(parsedEntry) {
    this.parsedEntry_ = parsedEntry;
  }

  /** @override */
  parseCustomDnsEntry(entry) {
    this.methodCalled('parseCustomDnsEntry', entry);
    return Promise.resolve(this.parsedEntry_);
  }

  /**
   * Sets the return values for probes to each template
   * @param {!Object<string, boolean>} results
   */
  setProbeResults(results) {
    this.probeResults_ = results;
  }

  /** @override */
  probeCustomDnsTemplate(template) {
    this.methodCalled('probeCustomDnsTemplate', template);
    // Prohibit unexpected probes.
    assertFalse(this.probeResults_[template] === undefined);
    return Promise.resolve(this.probeResults_[template]);
  }

  /** @override */
  recordUserDropdownInteraction(oldSelection, newSelection) {
    this.methodCalled(
        'recordUserDropdownInteraction', [oldSelection, newSelection]);
  }
}
