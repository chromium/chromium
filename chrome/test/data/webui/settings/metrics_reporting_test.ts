// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsPersonalizationOptionsElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrivacyPageBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';

// clang-format on

suite('MetricsReporting', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let page: SettingsPersonalizationOptionsElement;

  setup(function() {
    loadTimeData.overrideValues({shouldUseMetricsConsentRestructure: true});
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-personalization-options');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('hidden when metrics consent restructure is enabled', function() {
    assertFalse(isChildVisible(page, '#metricsReportingControl'));
  });
});

suite('MetricsConsentRestructureDisabled', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let page: SettingsPersonalizationOptionsElement;

  setup(function() {
    loadTimeData.overrideValues({shouldUseMetricsConsentRestructure: false});
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-personalization-options');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test(
      'changes to whether metrics reporting is enabled/managed',
      async function() {
        await testBrowserProxy.whenCalled('getMetricsReporting');
        await microtasksFinished();

        const control = page.shadowRoot.querySelector<CrToggleElement>(
            '#metricsReportingControl');
        assertTrue(!!control);
        assertEquals(
            testBrowserProxy.metricsReporting.enabled, control.checked);
        assertEquals(
            testBrowserProxy.metricsReporting.managed, control.disabled);
        assertEquals(
            testBrowserProxy.metricsReporting.managed,
            isChildVisible(page, 'cr-policy-indicator'));

        const changedMetrics = {
          enabled: !testBrowserProxy.metricsReporting.enabled,
          managed: !testBrowserProxy.metricsReporting.managed,
        };
        webUIListenerCallback('metrics-reporting-change', changedMetrics);
        await microtasksFinished();

        assertEquals(changedMetrics.enabled, control.checked);
        assertEquals(changedMetrics.managed, control.disabled);
        assertEquals(
            changedMetrics.managed,
            isChildVisible(page, 'cr-policy-indicator'));

        control.click();

        const enabled =
            await testBrowserProxy.whenCalled('setMetricsReportingEnabled');
        assertEquals(!changedMetrics.enabled, enabled);
      });

  test('metrics reporting restart button', async function() {
    await testBrowserProxy.whenCalled('getMetricsReporting');
    await microtasksFinished();

    // Restart button should be hidden by default (in any state).
    assertFalse(!!page.shadowRoot.querySelector('#restart'));

    // Simulate toggling via policy.
    webUIListenerCallback('metrics-reporting-change', {
      enabled: false,
      managed: true,
    });
    await microtasksFinished();

    // No restart button should show because the value is managed.
    assertFalse(!!page.shadowRoot.querySelector('#restart'));

    webUIListenerCallback('metrics-reporting-change', {
      enabled: true,
      managed: true,
    });
    await microtasksFinished();

    // Changes in policy should not show the restart button because the value
    // is still managed.
    assertFalse(!!page.shadowRoot.querySelector('#restart'));

    // Remove the policy and toggle the value.
    webUIListenerCallback('metrics-reporting-change', {
      enabled: false,
      managed: false,
    });
    await microtasksFinished();

    // Now the restart button should be showing.
    assertTrue(!!page.shadowRoot.querySelector('#restart'));

    // Receiving the same values should have no effect.
    webUIListenerCallback('metrics-reporting-change', {
      enabled: false,
      managed: false,
    });
    await microtasksFinished();
    assertTrue(!!page.shadowRoot.querySelector('#restart'));
  });
});
