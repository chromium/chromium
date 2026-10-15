// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsCollapseRadioButtonElement, V8PageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, SafeBrowsingSetting, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

function getInitialPrefs() {
  return [
    {
      key: 'generated.safe_browsing',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: SafeBrowsingSetting.STANDARD,
    },
    {
      key: 'generated.javascript_optimizer',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ContentSetting.ALLOW,
    },
  ];
}

function queryBlockOnUnfamiliarSitesRadioButton(page: HTMLElement) {
  return page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
      '#blockForUnfamiliarSites');
}

suite('V8Page', function() {
  let page: V8PageElement;
  let prefService: PrefService;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();
  });

  teardown(function() {
    page?.remove();
  });

  async function createPage(safeBrowsingSetting: SafeBrowsingSetting) {
    await prefService.setPrefValue(
        'generated.safe_browsing', safeBrowsingSetting);
    await prefService.setPrefValue(
        'generated.javascript_optimizer', ContentSetting.ALLOW);
    if (safeBrowsingSetting === SafeBrowsingSetting.DISABLED) {
      prefsBrowserProxy.fakeApi.sendPrefChanges([{
        key: 'generated.javascript_optimizer',
        value: ContentSetting.ALLOW,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
        controlledBy: chrome.settingsPrivate.ControlledBy.SAFE_BROWSING_OFF,
      }]);
    }
    const page = document.createElement('settings-v8-page');
    document.body.appendChild(page);
    await flushTasks();
    return page;
  }

  test('CheckRadioButtons_SafeBrowsingEnabled', async function() {
    page = await createPage(SafeBrowsingSetting.STANDARD);
    const radioButton = queryBlockOnUnfamiliarSitesRadioButton(page);
    assertTrue(!!radioButton);
    assertTrue(isVisible(radioButton));
    assertFalse(radioButton.disabled);
  });

  test('CheckRadioButtons_SafeBrowsingDisabled', async function() {
    page = await createPage(SafeBrowsingSetting.DISABLED);
    const radioButton = queryBlockOnUnfamiliarSitesRadioButton(page);
    assertTrue(!!radioButton);
    assertTrue(isVisible(radioButton));
    assertTrue(radioButton.disabled);
  });
});
