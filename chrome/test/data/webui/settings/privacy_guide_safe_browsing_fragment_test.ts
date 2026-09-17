// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {PrivacyGuideSafeBrowsingFragmentElement, SettingsCollapseRadioButtonElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {SafeBrowsingSetting} from 'chrome://settings/lazy_load.js';
import {loadTimeData, MetricsBrowserProxyImpl, PrefService, PrefsBrowserProxy, PrivacyGuideSettingsStates, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

suite('SafeBrowsingFragment', function() {
  let fragment: PrivacyGuideSafeBrowsingFragmentElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    const prefsBrowserProxy = new TestPrefsBrowserProxy([{
      key: 'generated.safe_browsing',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: SafeBrowsingSetting.STANDARD,
    }]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    fragment = document.createElement('privacy-guide-safe-browsing-fragment');
    document.body.appendChild(fragment);
  });

  async function assertSafeBrowsingMetrics({
    safeBrowsingStartsEnhanced,
    changeSetting,
    expectedMetric,
  }: {
    safeBrowsingStartsEnhanced: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    const safeBrowsingStartState = safeBrowsingStartsEnhanced ?
        SafeBrowsingSetting.ENHANCED :
        SafeBrowsingSetting.STANDARD;
    prefService.setPrefValue('generated.safe_browsing', safeBrowsingStartState);

    // The fragment is informed that it becomes visible by a receiving
    // a view-enter-start event.
    fragment.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true, composed: true}));

    if (changeSetting) {
      const radio = fragment.shadowRoot.querySelector<HTMLElement>(
          safeBrowsingStartsEnhanced ? '#safeBrowsingRadioStandard' :
                                       '#safeBrowsingRadioEnhanced');
      assertTrue(!!radio);
      radio.click();
      await microtasksFinished();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          safeBrowsingStartsEnhanced ?
              'Settings.PrivacyGuide.ChangeSafeBrowsingStandard' :
              'Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced');
    }

    // The fragment is informed that it becomes invisible by
    // receiving a view-enter-finish event.
    fragment.dispatchEvent(
        new CustomEvent('view-exit-finish', {bubbles: true, composed: true}));

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('EnhancedProtectionPrivacyGuide', async () => {
    const enhancedProtection =
        fragment.shadowRoot.querySelector<SettingsCollapseRadioButtonElement>(
            '#safeBrowsingRadioEnhanced');
    assertTrue(!!enhancedProtection);
    const epSubLabel =
        loadTimeData.getString('safeBrowsingEnhancedDescUpdated');
    assertEquals(epSubLabel, enhancedProtection.subLabel);

    const group = fragment.shadowRoot.querySelector<HTMLElement>(
        '#safeBrowsingRadioGroup');
    assertTrue(!!group);
    enhancedProtection.click();
    await eventToPromise('change', group);
    // The updated description item container should be visible.
    assertTrue(isChildVisible(fragment, '#updatedDescItemContainer'));
  });

  test('safeBrowsingMetricsEnhancedToStandard', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: true,
      changeSetting: true,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD,
    });
  });

  test('safeBrowsingMetricsStandardToEnhanced', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: false,
      changeSetting: true,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED,
    });
  });

  test('safeBrowsingMetricsStandardToStandard', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: false,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD,
    });
  });

  test('safeBrowsingMetricsEnhancedToEnhanced', function() {
    return assertSafeBrowsingMetrics({
      safeBrowsingStartsEnhanced: true,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED,
    });
  });

  test('fragmentUpdatesFromSafeBrowsingChanges', async function() {
    const radioButtonGroup =
        fragment.shadowRoot.querySelector<SettingsRadioGroupElement>(
            '#safeBrowsingRadioGroup');
    assertTrue(!!radioButtonGroup);

    prefService.setPrefValue(
        'generated.safe_browsing', SafeBrowsingSetting.ENHANCED);
    await microtasksFinished();
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.ENHANCED);

    prefService.setPrefValue(
        'generated.safe_browsing', SafeBrowsingSetting.STANDARD);
    await microtasksFinished();
    assertEquals(
        Number(radioButtonGroup.selected), SafeBrowsingSetting.STANDARD);
  });

  suite('HashPrefixRealTimeDisabled', function() {
    suiteSetup(function() {
      loadTimeData.overrideValues({
        enableHashPrefixRealTimeLookups: false,
      });
      resetRouterForTesting();
    });

    test('StandardProtectionDescription', function() {
      const standardProtection =
          fragment.shadowRoot.querySelector<SettingsCollapseRadioButtonElement>(
              '#safeBrowsingRadioStandard');
      assertTrue(!!standardProtection);
      const spSubLabel = loadTimeData.getString('safeBrowsingStandardDesc');
      assertEquals(spSubLabel, standardProtection.subLabel);
      assertTrue(standardProtection.noCollapse);
      assertFalse(isChildVisible(
          fragment, '#whenOnThingsToConsiderStandardProtection'));
    });
  });

  // <if expr="_google_chrome">
  suite('HashPrefixRealTimeEnabled', function() {
    suiteSetup(function() {
      loadTimeData.overrideValues({
        enableHashPrefixRealTimeLookups: true,
      });
      resetRouterForTesting();
    });

    test('StandardProtectionDescriptionWithProxy', function() {
      const standardProtection =
          fragment.shadowRoot.querySelector<SettingsCollapseRadioButtonElement>(
              '#safeBrowsingRadioStandard');
      assertTrue(!!standardProtection);
      const spSubLabel =
          loadTimeData.getString('safeBrowsingStandardDescProxy');
      assertEquals(spSubLabel, standardProtection.subLabel);
    });
  });
  // </if>
});
