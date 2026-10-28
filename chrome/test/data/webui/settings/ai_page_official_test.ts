// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import {OnDeviceAiBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl, PrefsBrowserProxy, PrefService, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import type {CrLinkRowElement, SettingsAiPageElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestOnDeviceAiBrowserProxy} from './test_on_device_ai_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('settings ai page official', function() {
  let testBrowserProxy: TestOnDeviceAiBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let aiPage: SettingsAiPageElement;

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    aiPage = document.createElement('settings-ai-page');
    document.body.appendChild(aiPage);
  }

  setup(function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy([]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    testBrowserProxy = new TestOnDeviceAiBrowserProxy();
    OnDeviceAiBrowserProxyImpl.setInstance(testBrowserProxy);
    loadTimeData.overrideValues({
      showOnDeviceAiAdvancedSettings: false,
    });
    resetRouterForTesting();
    createPage();
  });

  teardown(function() {
    openWindowProxy.reset();
  });

  function queryOnDeviceAiToggle(): SettingsToggleButtonElement|null {
    return aiPage.shadowRoot.querySelector<SettingsToggleButtonElement>(
        '#onDeviceAiToggle');
  }

  function getOnDeviceAiToggle(): SettingsToggleButtonElement {
    const toggle = queryOnDeviceAiToggle();
    assertTrue(!!toggle);
    return toggle;
  }

  function queryOnDeviceAiRow(): CrLinkRowElement|null {
    return aiPage.shadowRoot.querySelector<CrLinkRowElement>('#onDeviceAiRow');
  }

  test('show onDeviceAi toggle', async function() {
    await microtasksFinished();

    const toggle = getOnDeviceAiToggle();
    assertTrue(isVisible(toggle));
    assertTrue(toggle.checked);
    assertNull(queryOnDeviceAiRow());

    // Disable the setting.
    testBrowserProxy.setGetOnDeviceAiEnabledResponse({
      enabled: false,
      allowedByPolicy: true,
    });
    toggle.click();
    await microtasksFinished();
    assertFalse(toggle.checked);
    assertFalse(await testBrowserProxy.whenCalled('setOnDeviceAiEnabled'));
    assertFalse((await testBrowserProxy.getOnDeviceAiEnabled()).enabled);

    // Re-enable the setting.
    testBrowserProxy.resetResolver('setOnDeviceAiEnabled');
    testBrowserProxy.setGetOnDeviceAiEnabledResponse({
      enabled: true,
      allowedByPolicy: true,
    });
    toggle.click();
    await microtasksFinished();
    assertTrue(toggle.checked);
    assertTrue(await testBrowserProxy.whenCalled('setOnDeviceAiEnabled'));
    assertTrue((await testBrowserProxy.getOnDeviceAiEnabled()).enabled);
  });

  test(
      'show onDeviceAi link row when advanced settings enabled',
      async function() {
        loadTimeData.overrideValues({
          showAiPage: true,
          showOnDeviceAiAdvancedSettings: true,
        });
        resetRouterForTesting();
        createPage();
        await microtasksFinished();

        assertNull(queryOnDeviceAiToggle());
        const row = queryOnDeviceAiRow();
        assertTrue(!!row);
        assertTrue(isVisible(row));

        row.click();
        assertEquals(
            routes.ON_DEVICE_AI, Router.getInstance().getCurrentRoute());
      });

  test('onDeviceAi toggle disabled by policy', async function() {
    testBrowserProxy.setGetOnDeviceAiEnabledResponse({
      enabled: true,
      allowedByPolicy: false,
    });
    createPage();
    await testBrowserProxy.whenCalled('getOnDeviceAiEnabled');
    await microtasksFinished();

    const toggle = getOnDeviceAiToggle();
    assertTrue(isVisible(toggle));
    assertTrue(toggle.$.control.disabled);
    assertFalse(toggle.checked);
    const policyIndicator =
        toggle.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertTrue(!!policyIndicator);
    assertTrue(isVisible(policyIndicator));
  });

  test('onDeviceAi learn more link', async function() {
    await microtasksFinished();

    const toggle = getOnDeviceAiToggle();
    toggle.dispatchEvent(new CustomEvent('sub-label-link-clicked'));

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString('onDeviceAiLearnMoreUrl'), url);
  });

  test('onDeviceAi send feedback', async function() {
    await microtasksFinished();

    const toggle = getOnDeviceAiToggle();
    const section = toggle.closest('settings-section');
    assertTrue(!!section);

    const feedbackButton =
        section.shadowRoot.querySelector<HTMLElement>('#feedback');
    assertTrue(!!feedbackButton);

    feedbackButton.click();
    await testBrowserProxy.whenCalled('openFeedbackDialog');
  });
});
