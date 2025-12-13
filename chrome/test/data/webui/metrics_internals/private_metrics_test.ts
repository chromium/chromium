// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://metrics-internals/app.js';

import {MetricsInternalsBrowserProxyImpl} from 'chrome://metrics-internals/browser_proxy.js';
import type {FieldTrialState, HashNameMap, KeyValue, MetricsInternalsBrowserProxy, SeedType} from 'chrome://metrics-internals/browser_proxy.js';
import type {CwtKeyInfo, PrivateMetricsAppElement} from 'chrome://metrics-internals/private_metrics.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

function wait(): Promise<void> {
  return new Promise(resolve => {
    window.setTimeout(() => {
      resolve();
    }, 1);
  });
}

class FakeBrowser extends TestBrowserProxy implements
    MetricsInternalsBrowserProxy {
  cwtKeyInfo: CwtKeyInfo = {};

  constructor() {
    super([
      'getUmaLogData',
      'fetchVariationsSummary',
      'fetchStoredSeedInfo',
      'fetchUmaSummary',
      'isUsingMetricsServiceObserver',
      'setTrialEnrollState',
      'fetchTrialState',
      'lookupTrialOrGroupName',
      'fetchEncryptionPublicKey',
      'restart',
    ]);
  }

  // Note: Each browser proxy method uses `wait()` so that the returned
  // promise isn't immediately resolved. This mirrors the actual browser
  // proxy more accurately.

  async getUmaLogData(includeLogProtoData: boolean): Promise<string> {
    this.methodCalled('getUmaLogData', includeLogProtoData);
    await wait();
    return '';
  }

  async fetchVariationsSummary(): Promise<KeyValue[]> {
    this.methodCalled('fetchVariationsSummary');
    await wait();
    return [];
  }

  async fetchStoredSeedInfo(seedType: SeedType): Promise<KeyValue[]> {
    this.methodCalled(`fetchStored${seedType}SeedInfo`);
    await wait();
    return [];
  }

  async fetchUmaSummary(): Promise<KeyValue[]> {
    this.methodCalled('fetchUmaSummary');
    await wait();
    return [];
  }

  async isUsingMetricsServiceObserver(): Promise<boolean> {
    this.methodCalled('isUsingMetricsServiceObserver');
    await wait();
    return false;
  }

  async setTrialEnrollState(
      _trialHash: string, _groupHash: string,
      _forceEnable: boolean): Promise<boolean> {
    this.methodCalled('setTrialEnrollState');
    await wait();
    return false;
  }

  async fetchTrialState(): Promise<FieldTrialState> {
    this.methodCalled('fetchTrialState');
    await wait();
    return {trials: [], restartRequired: false};
  }

  async lookupTrialOrGroupName(_name: string): Promise<HashNameMap> {
    this.methodCalled('lookupTrialOrGroupName');
    await wait();
    return {};
  }

  async fetchEncryptionPublicKey(): Promise<CwtKeyInfo> {
    this.methodCalled('fetchEncryptionPublicKey');
    await wait();
    return this.cwtKeyInfo;
  }

  async restart(): Promise<void> {
    this.methodCalled('restart');
    await wait();
  }
}

suite('PrivateMetricsTest', function() {
  let app: PrivateMetricsAppElement;
  let fakeBrowser: FakeBrowser;

  async function waitForUpdate() {
    await new Promise<void>(resolve => {
      app.onUpdateForTesting = () => {
        resolve();
      };
    });
  }

  async function makeApp() {
    app = document.createElement('private-metrics-app');
    document.body.replaceChildren(app);
    await waitForUpdate();
  }

  function getDisplayedSummary(): Map<string, string> {
    const summary = new Map();
    const table = app.shadowRoot!.querySelector('#private-metrics-summary')!;
    for (const row of table.querySelectorAll('tr')) {
      const key = row.querySelector('td:first-child')!.textContent.trim();
      const value = row.querySelector('td:last-child')!.textContent.trim();
      summary.set(key, value);
    }
    return summary;
  }

  setup(() => {
    MetricsInternalsBrowserProxyImpl.setInstance(
        fakeBrowser = new FakeBrowser());
  });

  teardown(() => {
    MetricsInternalsBrowserProxyImpl.setInstance(
        new MetricsInternalsBrowserProxyImpl());
  });

  test('page loads and populates initial data', async function() {
    fakeBrowser.cwtKeyInfo = {
      issued_at: '1725555555000',
      expiration_time: '1725666666000',
      algorithm: -7,
      config_properties: 'config_properties_value',
      access_policy: 'access_policy_value',
      signature: 'signature_value',
      key_id: 'key_id_value',
      key_algorithm: -65537,
      key_curve: 9,
      key_ops: [1, 2, 3],
      key_x: 'key_x_value',
      key_d: 'key_d_value',
    };

    await makeApp();

    const summary = getDisplayedSummary();

    assertEquals(
        new Date(1725555555000)
            .toLocaleString(undefined, {dateStyle: 'long', timeStyle: 'long'}),
        summary.get('Issued At'));
    assertEquals(
        new Date(1725666666000)
            .toLocaleString(undefined, {dateStyle: 'long', timeStyle: 'long'}),
        summary.get('Expiration Time'));
    assertEquals('-7', summary.get('Algorithm'));
    assertEquals('config_properties_value', summary.get('Config Properties'));
    assertEquals('access_policy_value', summary.get('Access Policy'));
    assertEquals('signature_value', summary.get('Signature'));
    assertEquals('key_id_value', summary.get('Key ID'));
    assertEquals('-65537', summary.get('Key Algorithm'));
    assertEquals('9', summary.get('Key Curve'));
    assertEquals('1,2,3', summary.get('Key Ops'));
    assertEquals('key_x_value', summary.get('Key X'));
    assertEquals('key_d_value', summary.get('Key D'));
  });
});
