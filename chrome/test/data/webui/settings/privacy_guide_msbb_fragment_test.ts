// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {PrivacyGuideMsbbFragmentElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, MetricsBrowserProxyImpl, PrivacyGuideSettingsStates} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
// clang-format on


suite('MsbbFragment', function() {
  let fragment: PrivacyGuideMsbbFragmentElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    fragment = document.createElement('privacy-guide-msbb-fragment');
    fragment.prefs = settingsPrefs.prefs!;
    document.body.appendChild(fragment);

    return flushTasks();
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
    fragment.set(
        'prefs.url_keyed_anonymized_data_collection.enabled.value',
        msbbStartOn);

    // The fragment is informed that it becomes visible by a receiving
    // a view-enter-start event.
    fragment.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true, composed: true}));

    if (changeSetting) {
      fragment.shadowRoot!.querySelector<HTMLElement>(
                              '#urlCollectionToggle')!.click();
      flush();
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
