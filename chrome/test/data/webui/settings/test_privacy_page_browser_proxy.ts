// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {MetricsReporting, PrivacyPageBrowserProxy, ResolverOption, SecureDnsSetting} from 'chrome://settings/settings.js';
import {SecureDnsMode, SecureDnsUiManagementMode} from 'chrome://settings/settings.js';
import {assertFalse} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// clang-format on

export class TestPrivacyPageBrowserProxy extends TestBrowserProxy implements
    PrivacyPageBrowserProxy {
  metricsReporting: MetricsReporting;
  secureDnsSetting: SecureDnsSetting;
  private resolverList_: ResolverOption[];
  private isValidConfigResults_: {[config: string]: boolean} = {};
  private probeConfigResults_: {[config: string]: boolean} = {};

  constructor() {
    super([
      'getMetricsReporting',
      'setMetricsReportingEnabled',
      'showManageSslCertificates',
      'setBlockAutoplayEnabled',
      'getSecureDnsResolverList',
      'getSecureDnsSetting',
      'isValidConfig',
      'probeConfig',
    ]);

    this.metricsReporting = {
      enabled: true,
      managed: true,
    };

    this.secureDnsSetting = {
      mode: SecureDnsMode.AUTOMATIC,
      config: '',
      managementMode: SecureDnsUiManagementMode.NO_OVERRIDE,
      // <if expr="chromeos_ash">
      osMode: SecureDnsMode.AUTOMATIC,
      osConfig: '',
      dohWithIdentifiersActive: false,
      configForDisplay: '',
      dohDomainConfigSet: false,
      // </if>
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

  showManageSslCertificates() {
    this.methodCalled('showManageSslCertificates');
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
   * Sets the return value for the next isValidConfig call.
   */
  setIsValidConfigResult(entry: string, result: boolean) {
    this.isValidConfigResults_[entry] = result;
  }

  isValidConfig(entry: string): Promise<boolean> {
    this.methodCalled('isValidConfig', entry);
    // Prohibit unexpected validations.
    const result = this.isValidConfigResults_[entry];
    assertFalse(result === undefined);
    return Promise.resolve(result || false);
  }

  /**
   * Sets the return value for the next probeConfig call.
   */
  setProbeConfigResult(entry: string, result: boolean) {
    this.probeConfigResults_[entry] = result;
  }

  probeConfig(entry: string): Promise<boolean> {
    this.methodCalled('probeConfig', entry);
    // Prohibit unexpected probes.
    const result = this.probeConfigResults_[entry];
    assertFalse(result === undefined);
    return Promise.resolve(result || false);
  }
}
