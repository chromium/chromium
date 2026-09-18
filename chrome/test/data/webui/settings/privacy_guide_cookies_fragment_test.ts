// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {PrivacyGuideCookiesFragmentElement, SettingsCollapseRadioButtonElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {ThirdPartyCookieBlockingSetting} from 'chrome://settings/lazy_load.js';
import {loadTimeData, MetricsBrowserProxyImpl, PrefService, PrefsBrowserProxy, PrivacyGuideSettingsStates} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

suite('CookiesFragment', function() {
  let fragment: PrivacyGuideCookiesFragmentElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let prefService: PrefService;

  setup(async function() {
    assertTrue(!!window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    const prefsBrowserProxy = new TestPrefsBrowserProxy([{
      key: 'generated.third_party_cookie_blocking_setting',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY,
    }]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    fragment = document.createElement('privacy-guide-cookies-fragment');
    document.body.appendChild(fragment);
  });

  async function assertCookieMetrics({
    cookieStartsBlock3PIncognito,
    changeSetting,
    expectedMetric,
  }: {
    cookieStartsBlock3PIncognito: boolean,
    changeSetting: boolean,
    expectedMetric: PrivacyGuideSettingsStates,
  }) {
    const cookieStartState = cookieStartsBlock3PIncognito ?
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY :
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY;
    prefService.setPrefValue(
        'generated.third_party_cookie_blocking_setting', cookieStartState);

    // The fragment is informed that it becomes visible by a receiving
    // a view-enter-start event.
    fragment.fire('view-enter-start');

    if (changeSetting) {
      const buttonId =
          cookieStartsBlock3PIncognito ? '#block3pcs' : '#allow3pcs';
      const button = fragment.shadowRoot.querySelector<HTMLElement>(buttonId);
      assertTrue(!!button);
      button.click();
      await microtasksFinished();
      const actionResult =
          await testMetricsBrowserProxy.whenCalled('recordAction');
      assertEquals(
          actionResult,
          cookieStartsBlock3PIncognito ?
              'Settings.PrivacyGuide.ChangeCookiesBlock3P' :
              'Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito');
    }

    // The fragment is informed that it becomes invisible by
    // receiving a view-enter-finish event.
    fragment.dispatchEvent(
        new CustomEvent('view-exit-finish', {bubbles: true, composed: true}));

    const result = await testMetricsBrowserProxy.whenCalled(
        'recordPrivacyGuideSettingsStatesHistogram');
    assertEquals(result, expectedMetric);
  }

  test('cookiesMetrics3PIncognitoTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P_INCOGNITO,
    });
  });

  test('cookiesMetrics3PIncognitoTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P,
    });
  });

  test('cookiesMetrics3PTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P_INCOGNITO,
    });
  });

  test('cookiesMetrics3PTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P,
    });
  });

  test('fragmentUpdatesFromCookieChanges', async function() {
    const radioButtonGroup =
        fragment.shadowRoot.querySelector<SettingsRadioGroupElement>(
            '#cookiesRadioGroup');
    assertTrue(!!radioButtonGroup);

    prefService.setPrefValue(
        'generated.third_party_cookie_blocking_setting',
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
    await microtasksFinished();
    assertEquals(
        Number(radioButtonGroup.selected),
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);

    prefService.setPrefValue(
        'generated.third_party_cookie_blocking_setting',
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    await microtasksFinished();
    assertEquals(
        Number(radioButtonGroup.selected),
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
  });

  test('showsAllow3pcsAndBlock3pcsLabels', function() {
    const allow3pcsLabel =
        fragment.shadowRoot.querySelector<SettingsCollapseRadioButtonElement>(
            '#allow3pcs');
    assertTrue(!!allow3pcsLabel);

    assertEquals(
        loadTimeData.getString('privacyGuideCookiesCardBlockTpcAllowSubheader'),
        allow3pcsLabel.label);

    const block3pcsLabel =
        fragment.shadowRoot.querySelector<SettingsCollapseRadioButtonElement>(
            '#block3pcs');
    assertTrue(!!block3pcsLabel);
    assertEquals(
        loadTimeData.getString('privacyGuideCookiesCardBlockTpcBlockSubheader'),
        block3pcsLabel.label);
  });
});
