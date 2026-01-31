// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OnDeviceAiBrowserProxy, SettingsSystemPageElement} from 'chrome://settings/lazy_load.js';
import {OnDeviceAiBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

class TestOnDeviceAiBrowserProxy extends TestBrowserProxy implements
    OnDeviceAiBrowserProxy {
  private onDeviceAiEnabled_: boolean = true;

  constructor() {
    super(['getOnDeviceAiEnabled', 'setOnDeviceAiEnabled']);
  }

  getOnDeviceAiEnabled() {
    this.methodCalled('getOnDeviceAiEnabled');
    return Promise.resolve({enabled: this.onDeviceAiEnabled_});
  }

  setGetOnDeviceAiEnabledResponse(enabled: boolean) {
    this.onDeviceAiEnabled_ = enabled;
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
    testBrowserProxy = new TestOnDeviceAiBrowserProxy();
    OnDeviceAiBrowserProxyImpl.setInstance(testBrowserProxy);

    systemPage = document.createElement('settings-system-page');
    document.body.appendChild(systemPage);
  }

  setup(function() {
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
    testBrowserProxy.setGetOnDeviceAiEnabledResponse(false);
    toggle.click();
    await flushTasks();
    assertFalse(toggle.checked);
    assertFalse(await testBrowserProxy.whenCalled('setOnDeviceAiEnabled'));
    assertFalse((await testBrowserProxy.getOnDeviceAiEnabled()).enabled);

    // Re-enable the setting.
    testBrowserProxy.resetResolver('setOnDeviceAiEnabled');
    testBrowserProxy.setGetOnDeviceAiEnabledResponse(true);
    toggle.click();
    await flushTasks();
    assertTrue(toggle.checked);
    assertTrue(await testBrowserProxy.whenCalled('setOnDeviceAiEnabled'));
    assertTrue((await testBrowserProxy.getOnDeviceAiEnabled()).enabled);
  });

  test('hide onDeviceAi toggle by default', function() {
    assertFalse(loadTimeData.getBoolean('showOnDeviceAiSettings'));
    assertNull(queryOnDeviceAiToggle());
  });
});
