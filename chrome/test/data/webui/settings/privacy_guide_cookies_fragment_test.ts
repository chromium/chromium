// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {PrivacyGuideCookiesFragmentElement, SettingsCollapseRadioButtonElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {ThirdPartyCookieBlockingSetting} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, MetricsBrowserProxyImpl, PrivacyGuideSettingsStates} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

async function assertCookieMetrics({
  cookieStartsBlock3PIncognito,
  changeSetting,
  expectedMetric,
  fragment,
  testMetricsBrowserProxy,
}: {
  cookieStartsBlock3PIncognito: boolean,
  changeSetting: boolean,
  expectedMetric: PrivacyGuideSettingsStates,
  fragment: PrivacyGuideCookiesFragmentElement,
  testMetricsBrowserProxy: TestMetricsBrowserProxy,
}) {
  const cookieStartState = cookieStartsBlock3PIncognito ?
      ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY :
      ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY;
  fragment.set(
      'prefs.generated.third_party_cookie_blocking_setting.value',
      cookieStartState);

  // The fragment is informed that it becomes visible by a receiving
  // a view-enter-start event.
  fragment.dispatchEvent(
      new CustomEvent('view-enter-start', {bubbles: true, composed: true}));

  if (changeSetting) {
    const buttonId = cookieStartsBlock3PIncognito ? '#block3pcs' : '#allow3pcs';
    const button = fragment.shadowRoot!.querySelector<HTMLElement>(buttonId);
    assertTrue(!!button);
    button.click();
    flush();
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

suite('CookiesFragment', function() {
  let fragment: PrivacyGuideCookiesFragmentElement;
  let settingsPrefs: SettingsPrefsElement;
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    assertTrue(!!window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;

    assertTrue(loadTimeData.getBoolean('showPrivacyGuide'));
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);

    fragment = document.createElement('privacy-guide-cookies-fragment');
    fragment.prefs = settingsPrefs.prefs!;
    document.body.appendChild(fragment);

    return flushTasks();
  });

  test('cookiesMetrics3PIncognitoTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: false,
      expectedMetric:
          PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P_INCOGNITO,
      fragment,
      testMetricsBrowserProxy,
    });
  });

  test('cookiesMetrics3PIncognitoTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: true,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P,
      fragment,
      testMetricsBrowserProxy,
    });
  });

  test('cookiesMetrics3PTo3PIncognito', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: true,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P_INCOGNITO,
      fragment,
      testMetricsBrowserProxy,
    });
  });

  test('cookiesMetrics3PTo3P', function() {
    return assertCookieMetrics({
      cookieStartsBlock3PIncognito: false,
      changeSetting: false,
      expectedMetric: PrivacyGuideSettingsStates.BLOCK_3P_TO_3P,
      fragment,
      testMetricsBrowserProxy,
    });
  });

  test('fragmentUpdatesFromCookieChanges', function() {
    const radioButtonGroup =
        fragment.shadowRoot!.querySelector<SettingsRadioGroupElement>(
            '#cookiesRadioGroup');
    assertTrue(!!radioButtonGroup);

    fragment.set(
        'prefs.generated.third_party_cookie_blocking_setting.value',
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);
    assertEquals(
        Number(radioButtonGroup.selected),
        ThirdPartyCookieBlockingSetting.BLOCK_THIRD_PARTY);

    fragment.set(
        'prefs.generated.third_party_cookie_blocking_setting.value',
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
    assertEquals(
        Number(radioButtonGroup.selected),
        ThirdPartyCookieBlockingSetting.INCOGNITO_ONLY);
  });

  test('showsAllow3pcsAndBlock3pcsLabels', function() {
    const allow3pcsLabel =
        fragment.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
            '#allow3pcs');
    assertTrue(!!allow3pcsLabel);

    assertEquals(
        loadTimeData.getString('privacyGuideCookiesCardBlockTpcAllowSubheader'),
        allow3pcsLabel.label);

    const block3pcsLabel =
        fragment.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
            '#block3pcs');
    assertTrue(!!block3pcsLabel);
    assertEquals(
        loadTimeData.getString('privacyGuideCookiesCardBlockTpcBlockSubheader'),
        block3pcsLabel.label);
  });
});
