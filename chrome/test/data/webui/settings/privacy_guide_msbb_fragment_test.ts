// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {PrivacyGuideMsbbFragmentElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, MetricsBrowserProxyImpl, PrefService, PrefsBrowserProxy, PrivacyGuideSettingsStates} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
// clang-format on

suite('MsbbFragment', function() {
  let fragment: PrivacyGuideMsbbFragmentElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    const prefsBrowserProxy = new TestPrefsBrowserProxy([{
      key: 'url_keyed_anonymized_data_collection.enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    }]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    fragment = document.createElement('privacy-guide-msbb-fragment');
    document.body.appendChild(fragment);
  });

  async function assertMsbbMetrics({
    msbbStartOn,
    changeSetting,
    expectedMetric,
  }: {
    msbbStartOn: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    prefService.setPrefValue(
        'url_keyed_anonymized_data_collection.enabled', msbbStartOn);

    // The fragment is informed that it becomes visible by a receiving
    // a view-enter-start event.
    fragment.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true, composed: true}));

    if (changeSetting) {
      const toggle = fragment.shadowRoot.querySelector<HTMLElement>(
          '#urlCollectionToggle');
      assertTrue(!!toggle);
      toggle.click();
      await microtasksFinished();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          msbbStartOn ? 'Settings.PrivacyGuide.ChangeMSBBOff' :
                        'Settings.PrivacyGuide.ChangeMSBBOn');
    }

    // The fragment is informed that it becomes invisible by
    // receiving a view-enter-finish event.
    fragment.dispatchEvent(
        new CustomEvent('view-exit-finish', {bubbles: true, composed: true}));

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('msbbMetricsOnToOn', function() {
    return assertMsbbMetrics({
      msbbStartOn: true,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_ON_TO_ON,
    });
  });

  test('msbbMetricsOnToOff', function() {
    return assertMsbbMetrics({
      msbbStartOn: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_ON_TO_OFF,
    });
  });

  test('msbbMetricsOffToOn', function() {
    return assertMsbbMetrics({
      msbbStartOn: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_OFF_TO_ON,
    });
  });

  test('msbbMetricsOffToOff', function() {
    return assertMsbbMetrics({
      msbbStartOn: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF,
    });
  });
});
