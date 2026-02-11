// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OnDeviceAiBrowserProxy, OnDeviceAiEnabled, SettingsSystemPageElement} from 'chrome://settings/lazy_load.js';
import {OnDeviceAiBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

class TestOnDeviceAiBrowserProxy extends TestBrowserProxy implements
    OnDeviceAiBrowserProxy {
  private onDeviceAiEnabled_: OnDeviceAiEnabled = {
    enabled: true,
    allowedByPolicy: true,
  };

  constructor() {
    super(['getOnDeviceAiEnabled', 'setOnDeviceAiEnabled']);
  }

  getOnDeviceAiEnabled() {
    this.methodCalled('getOnDeviceAiEnabled');
    return Promise.resolve(this.onDeviceAiEnabled_);
  }

  setGetOnDeviceAiEnabledResponse(response: OnDeviceAiEnabled) {
    this.onDeviceAiEnabled_ = response;
  }

  setOnDeviceAiEnabled(enabled: boolean) {
    this.methodCalled('setOnDeviceAiEnabled', enabled);
  }
}

suite('settings system page official', function() {
  let testBrowserProxy: TestOnDeviceAiBrowserProxy;
  let systemPage: SettingsSystemPageElement;

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    systemPage = document.createElement('settings-system-page');
    document.body.appendChild(systemPage);
  }

  setup(function() {
    testBrowserProxy = new TestOnDeviceAiBrowserProxy();
    OnDeviceAiBrowserProxyImpl.setInstance(testBrowserProxy);
    loadTimeData.overrideValues({
      showOnDeviceAiSettings: false,
    });
    createPage();
  });

  function queryOnDeviceAiToggle(): SettingsToggleButtonElement|null {
    // Toggle is behind a `dom-if`, so retrieve it via `querySelector`
    // (`systemPage.$` only contains static Polymer nodes).
    return systemPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#onDeviceAiToggle');
  }

  test('show onDeviceAi toggle', async function() {
    loadTimeData.overrideValues({
      showOnDeviceAiSettings: true,
    });
    createPage();
    await flushTasks();

    const toggle = queryOnDeviceAiToggle();
    assertTrue(!!toggle);
    assertTrue(isVisible(toggle));
    assertTrue(toggle.checked);

    // Disable the setting.
    testBrowserProxy.setGetOnDeviceAiEnabledResponse({
      enabled: false,
      allowedByPolicy: true,
    });
    toggle.click();
    await flushTasks();
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
    await flushTasks();
    assertTrue(toggle.checked);
    assertTrue(await testBrowserProxy.whenCalled('setOnDeviceAiEnabled'));
    assertTrue((await testBrowserProxy.getOnDeviceAiEnabled()).enabled);
  });

  test('onDeviceAi toggle disabled by policy', async function() {
    loadTimeData.overrideValues({
      showOnDeviceAiSettings: true,
    });
    testBrowserProxy.setGetOnDeviceAiEnabledResponse({
      enabled: true,
      allowedByPolicy: false,
    });
    createPage();
    await testBrowserProxy.whenCalled('getOnDeviceAiEnabled');
    await flushTasks();

    const toggle = queryOnDeviceAiToggle();
    assertTrue(!!toggle);
    assertTrue(isVisible(toggle));
    assertTrue(toggle.$.control.disabled);
    assertFalse(toggle.checked);
    const policyIndicator =
        toggle.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertTrue(!!policyIndicator);
    assertTrue(isVisible(policyIndicator));
  });

  test('hide onDeviceAi toggle by default', function() {
    assertFalse(loadTimeData.getBoolean('showOnDeviceAiSettings'));
    assertNull(queryOnDeviceAiToggle());
  });
});
