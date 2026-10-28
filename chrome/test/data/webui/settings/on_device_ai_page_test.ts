// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsOnDeviceAiPageElement} from 'chrome://settings/lazy_load.js';
import {OnDeviceAiBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl, PrefsBrowserProxy, PrefService, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestOnDeviceAiBrowserProxy} from './test_on_device_ai_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('OnDeviceAiPage', function() {
  let testBrowserProxy: TestOnDeviceAiBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let page: SettingsOnDeviceAiPageElement;

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-on-device-ai-page');
    document.body.appendChild(page);
    await testBrowserProxy.whenCalled('getOnDeviceAiEnabled');
    await microtasksFinished();
  }

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy([]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    loadTimeData.overrideValues({
      showAiPage: true,
      showOnDeviceAiAdvancedSettings: true,
    });
    resetRouterForTesting();

    testBrowserProxy = new TestOnDeviceAiBrowserProxy();
    OnDeviceAiBrowserProxyImpl.setInstance(testBrowserProxy);
    await createPage();
  });

  teardown(function() {
    openWindowProxy.reset();
  });

  test('toggle default on and updates browser proxy', async function() {
    assertTrue(isVisible(page.$.onDeviceAiToggle));
    assertTrue(page.$.onDeviceAiToggle.checked);

    page.$.onDeviceAiToggle.click();
    await microtasksFinished();
    assertFalse(page.$.onDeviceAiToggle.checked);
    assertFalse(await testBrowserProxy.whenCalled('setOnDeviceAiEnabled'));

    testBrowserProxy.resetResolver('setOnDeviceAiEnabled');
    page.$.onDeviceAiToggle.click();
    await microtasksFinished();
    assertTrue(page.$.onDeviceAiToggle.checked);
    assertTrue(await testBrowserProxy.whenCalled('setOnDeviceAiEnabled'));
  });

  test('disallowed by policy disables toggle', async function() {
    testBrowserProxy.reset();
    testBrowserProxy.setGetOnDeviceAiEnabledResponse({
      enabled: true,
      allowedByPolicy: false,
    });
    await createPage();

    assertTrue(page.$.onDeviceAiToggle.controlDisabled());
    assertTrue(page.$.onDeviceAiToggle.isPrefEnforced());
    assertFalse(page.$.onDeviceAiToggle.checked);
  });

  test('learnMoreLinkClicked', async function() {
    page.$.onDeviceAiToggle.dispatchEvent(
        new CustomEvent('sub-label-link-clicked'));

    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString('onDeviceAiLearnMoreUrl'), url);
  });

  test('webUiListenerUpdatesToggle', async function() {
    assertTrue(page.$.onDeviceAiToggle.checked);

    webUIListenerCallback('on-device-ai-enabled-changed', {
      enabled: false,
      allowedByPolicy: true,
    });
    await microtasksFinished();
    assertFalse(page.$.onDeviceAiToggle.checked);
  });
});
