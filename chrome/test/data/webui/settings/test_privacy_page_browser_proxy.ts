// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {MetricsReporting, PrivacyPageBrowserProxy, ResolverOption, SecureDnsMode, SecureDnsSetting, SecureDnsUiManagementMode} from 'chrome://settings/settings.js';
import {assertFalse} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

export class TestPrivacyPageBrowserProxy extends TestBrowserProxy implements
    PrivacyPageBrowserProxy {
  metricsReporting: MetricsReporting;
  secureDnsSetting: SecureDnsSetting;
  private resolverList_: ResolverOption[];
  private parsedEntry_: string[] = [];
  private probeResults_: {[template: string]: boolean} = {};

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

    this.metricsReporting = {
      enabled: true,
      managed: true,
    };

    this.secureDnsSetting = {
      mode: SecureDnsMode.AUTOMATIC,
      templates: [],
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
    };

    this.resolverList_ = [{name: 'Custom', value: 'custom', policy: ''}];
  }

  getMetricsReporting() {
    this.methodCalled('getMetricsReporting');
    return Promise.resolve(this.metricsReporting);
  }

  setMetricsReportingEnabled(enabled: boolean) {
    this.methodCalled('setMetricsReportingEnabled', enabled);
  }

  showManageSSLCertificates() {
    this.methodCalled('showManageSSLCertificates');
  }

  setBlockAutoplayEnabled(enabled: boolean) {
    this.methodCalled('setBlockAutoplayEnabled', enabled);
  }

  /**
   * Sets the resolver list that will be returned when getSecureDnsResolverList
   * is called.
   */
  setResolverList(resolverList: ResolverOption[]) {
    this.resolverList_ = resolverList;
  }

  getSecureDnsResolverList() {
    this.methodCalled('getSecureDnsResolverList');
    return Promise.resolve(this.resolverList_);
  }

  getSecureDnsSetting() {
    this.methodCalled('getSecureDnsSetting');
    return Promise.resolve(this.secureDnsSetting);
  }

  /**
   * Sets the return value for the next parseCustomDnsEntry call.
   */
  setParsedEntry(parsedEntry: string[]) {
    this.parsedEntry_ = parsedEntry;
  }

  parseCustomDnsEntry(entry: string) {
    this.methodCalled('parseCustomDnsEntry', entry);
    return Promise.resolve(this.parsedEntry_);
  }

  /**
   * Sets the return values for probes to each template
   */
  setProbeResults(results: {[template: string]: boolean}) {
    this.probeResults_ = results;
  }

  probeCustomDnsTemplate(template: string) {
    this.methodCalled('probeCustomDnsTemplate', template);
    // Prohibit unexpected probes.
    assertFalse(this.probeResults_[template] === undefined);
    return Promise.resolve(this.probeResults_[template]!);
  }

  recordUserDropdownInteraction(oldSelection: string, newSelection: string) {
    this.methodCalled(
        'recordUserDropdownInteraction', [oldSelection, newSelection]);
  }
}
