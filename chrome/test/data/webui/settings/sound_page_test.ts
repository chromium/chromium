// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SoundPageElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {loadTimeData, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
// clang-format on

suite('SoundPage', function() {
  let testSiteSettingsPrefsBrowserProxy: TestSiteSettingsBrowserProxy;
  let page: SoundPageElement;

  function getToggleElement(): SettingsToggleButtonElement {
    const toggle = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#blockAutoplaySetting');
    assertTrue(!!toggle);
    return toggle;
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: true});
    resetRouterForTesting();

    testSiteSettingsPrefsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(testSiteSettingsPrefsBrowserProxy);

    page = document.createElement('settings-sound-page');
    document.body.appendChild(page);
    return flushTasks();
  });

  test('UpdateStatus', async () => {
    assertTrue(getToggleElement().disabled);
    assertFalse(getToggleElement().checked);

    webUIListenerCallback('onBlockAutoplayStatusChanged', {
      pref: {value: true},
      enabled: true,
    });

    await flushTasks();
    // Check that we are on and enabled.
    assertFalse(getToggleElement().disabled);
    assertTrue(getToggleElement().checked);

    // Toggle the pref off.
    webUIListenerCallback('onBlockAutoplayStatusChanged', {
      pref: {value: false},
      enabled: true,
    });

    await flushTasks();
    // Check that we are off and enabled.
    assertFalse(getToggleElement().disabled);
    assertFalse(getToggleElement().checked);

    // Disable the autoplay status toggle.
    webUIListenerCallback('onBlockAutoplayStatusChanged', {
      pref: {value: false},
      enabled: false,
    });

    await flushTasks();
    // Check that we are off and disabled.
    assertTrue(getToggleElement().disabled);
    assertFalse(getToggleElement().checked);
  });

  test('Hidden', async () => {
    assertTrue(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
    assertFalse(getToggleElement().hidden);

    loadTimeData.overrideValues({enableBlockAutoplayContentSetting: false});
    resetRouterForTesting();

    page.remove();
    page = document.createElement('settings-sound-page');
    document.body.appendChild(page);

    await flushTasks();
    assertFalse(loadTimeData.getBoolean('enableBlockAutoplayContentSetting'));
    assertTrue(getToggleElement().hidden);
  });

  test('Click', async () => {
    assertTrue(getToggleElement().disabled);
    assertFalse(getToggleElement().checked);

    webUIListenerCallback('onBlockAutoplayStatusChanged', {
      pref: {value: true},
      enabled: true,
    });

    await flushTasks();
    // Check that we are on and enabled.
    assertFalse(getToggleElement().disabled);
    assertTrue(getToggleElement().checked);

    // Click on the toggle and wait for the proxy to be called.
    getToggleElement().click();
    const enabled = await testSiteSettingsPrefsBrowserProxy.whenCalled(
        'setBlockAutoplayEnabled');
    assertFalse(enabled);
  });
});
