// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {IncognitoTrackingProtectionsPageElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, Router, resetRouterForTesting,MetricsBrowserProxyImpl, PrivacyElementInteractions} from 'chrome://settings/settings.js';
import {assertTrue, assertFalse, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import 'chrome://settings/lazy_load.js';

suite('IncognitoTrackingProtectionsPageTest', function() {
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let page: IncognitoTrackingProtectionsPageElement;
  let settingsPrefs: SettingsPrefsElement;

  function createPage() {
    page = document.createElement('incognito-tracking-protections-page');
    page.prefs = settingsPrefs.prefs!;
    // Set prefs to enabled by default.
    page.set('prefs.tracking_protection.fingerprinting_protection_enabled.value', true);
    page.set('prefs.tracking_protection.ip_protection_enabled.value', true);
    document.body.appendChild(page);
    flush();
  }

  suiteSetup(function() {
    loadTimeData.overrideValues({
      isFingerprintingProtectionUxEnabled: true,
      isIpProtectionDisabledForEnterprise: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    resetRouterForTesting();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    createPage();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('ElementVisibility', function() {
    // Block 3PCs toggle should always be available.
    assertTrue(isChildVisible(page, '#block3pcsToggle'));
    // Fingerprinting protection will be available by default.
    assertTrue(isChildVisible(page, '#fingerprintingProtectionToggle'));
    // IP protection will not be available by default.
    assertFalse(isChildVisible(page, '#ipProtectionToggle'));
  });

  test('block3pcsToggleIsDisabledAndChecked', function() {
    const block3pcsToggle =
    page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#block3pcsToggle');
    assertTrue(!!block3pcsToggle);
    assertTrue(block3pcsToggle.hasAttribute('disabled'));
    assertTrue(block3pcsToggle.hasAttribute('checked'));
  });

  test('fppToggleEnablesPrefAndRecordsHistogram', async function() {
    page.set(
        'prefs.tracking_protection.fingerprinting_protection_enabled.value',
        false);
    const fingerprintingProtectionToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#fingerprintingProtectionToggle');
    assertTrue(!!fingerprintingProtectionToggle);
    fingerprintingProtectionToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.FINGERPRINTING_PROTECTION, result);
    assertEquals(
        page.getPref(
            'tracking_protection.fingerprinting_protection_enabled.value'),
        true);
  });

  // TODO(crbug.com/408036586): Remove when FingerprintingProtectionUx fully launched.
  test('ElementVisibility_isFingerprintingProtectionUx_disabled', async function() {
    loadTimeData.overrideValues({
      isFingerprintingProtectionUxEnabled: false,
    });
    resetRouterForTesting();
    await createPage();

    assertTrue(isChildVisible(page, '#block3pcsToggle'));
    assertFalse(isChildVisible(page, '#fingerprintingProtectionToggle'));
    assertFalse(isChildVisible(page, '#ipProtectionToggle'));
  });

  test('ElementVisibility_isIpProtectionUx_enabled', async function() {
    loadTimeData.overrideValues({
      isIpProtectionUxEnabled: true,
    });
    resetRouterForTesting();
    await createPage();

    assertTrue(isChildVisible(page, '#block3pcsToggle'));
    assertFalse(isChildVisible(page, '#fingerprintingProtectionToggle'));
    assertTrue(isChildVisible(page, '#ipProtectionToggle'));
  });

  test('ippToggleEnablesPrefAndRecordsHistogram', async function() {
    loadTimeData.overrideValues({
      isIpProtectionUxEnabled: true,
    });
    resetRouterForTesting();
    await createPage();

    page.set('prefs.tracking_protection.ip_protection_enabled.value', false);
    const ipProtectionToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#ipProtectionToggle');
    assertTrue(!!ipProtectionToggle);
    ipProtectionToggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.IP_PROTECTION, result);
    assertEquals(
        page.getPref('tracking_protection.ip_protection_enabled.value'), true);
  });

  test(
      'ippToggleDisabledAndUncheckedWhenIppDisabledForEnterprise',
      async function() {
        loadTimeData.overrideValues({
          isIpProtectionUxEnabled: true,
          isIpProtectionDisabledForEnterprise: true,
        });
        resetRouterForTesting();
        await createPage();

        const ipProtectionToggle =
            page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#ipProtectionToggle');
        assertTrue(!!ipProtectionToggle);
        assertTrue(ipProtectionToggle.disabled);
        assertFalse(ipProtectionToggle.checked);
      });
});
