// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsCollapseRadioButtonElement, V8PageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, SafeBrowsingSetting, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

function createPage(settingsPrefs: SettingsPrefsElement,
                    safeBrowsingSetting: SafeBrowsingSetting) {
  const page = document.createElement('settings-v8-page');
  page.prefs = settingsPrefs.prefs;

  document.body.appendChild(page);

  page.setPrefValue('generated.safe_browsing', safeBrowsingSetting);
  page.setPrefValue('generated.javascript_optimizer', ContentSetting.ALLOW);
  if (safeBrowsingSetting === SafeBrowsingSetting.DISABLED) {
    page.set(
        'prefs.generated.javascript_optimizer.enforcement',
        chrome.settingsPrivate.Enforcement.ENFORCED);
    page.set(
        'prefs.generated.javascript_optimizer.controlledBy',
        chrome.settingsPrivate.ControlledBy.SAFE_BROWSING_OFF);
  }
  return page;
}

function queryBlockOnUnfamiliarSitesRadioButton(page: HTMLElement) {
  return page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
      '#blockForUnfamiliarSites');
}

suite('V8Page_BlockOnUnfamiliarSitesFeatureDisabled', function() {
  let page: V8PageElement;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    assertFalse(
        loadTimeData.getBoolean('enableBlockV8OptimizerOnUnfamiliarSites'));

    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    page = createPage(settingsPrefs, SafeBrowsingSetting.STANDARD);
    return flushTasks();
  });

  test('CheckRadioButtons', function() {
    assertFalse(!!queryBlockOnUnfamiliarSitesRadioButton(page));
  });
});

suite('V8Page_BlockOnUnfamiliarSitesFeatureEnabled', function() {
  let page: V8PageElement;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    assertTrue(
        loadTimeData.getBoolean('enableBlockV8OptimizerOnUnfamiliarSites'));
  });

  test('CheckRadioButtons_SafeBrowsingEnabled', async function() {
    page = createPage(settingsPrefs, SafeBrowsingSetting.STANDARD);
    await flushTasks();
    const radioButton = queryBlockOnUnfamiliarSitesRadioButton(page);
    assertTrue(!!radioButton);
    assertTrue(isVisible(radioButton));
    assertFalse(radioButton.disabled);
  });

  test('CheckRadioButtons_SafeBrowsingDisabled', async function() {
    page = createPage(settingsPrefs, SafeBrowsingSetting.DISABLED);
    await flushTasks();
    const radioButton = queryBlockOnUnfamiliarSitesRadioButton(page);
    assertTrue(!!radioButton);
    assertTrue(isVisible(radioButton));
    assertTrue(radioButton.disabled);
  });
});
