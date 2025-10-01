// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsCollapseRadioButtonElement, V8PageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, SafeBrowsingSetting, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

function createPage(safeBrowsingSetting: SafeBrowsingSetting) {
  const page = document.createElement('settings-v8-page');
  const isSafeBrowsingDisabled =
      safeBrowsingSetting === SafeBrowsingSetting.DISABLED;
  page.prefs = {
    generated: {
      safe_browsing: {
        value: safeBrowsingSetting,
      },
      javascript_optimizer: {
        type: chrome.settingsPrivate.PrefType.NUMBER,
        key: 'test',
        value: ContentSetting.ALLOW,
        enforcement: isSafeBrowsingDisabled ?
            chrome.settingsPrivate.Enforcement.ENFORCED :
            undefined,
        controlledBy: isSafeBrowsingDisabled ?
            chrome.settingsPrivate.ControlledBy.SAFE_BROWSING_OFF :
            undefined,
      },
    },
  };

  document.body.appendChild(page);
  return page;
}

function queryBlockOnUnfamiliarSitesRadioButton(page: HTMLElement) {
  return page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
      '#blockForUnfamiliarSites');
}

suite('V8Page_BlockOnUnfamiliarSitesFeatureDisabled', function() {
  let page: V8PageElement;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    assertFalse(
        loadTimeData.getBoolean('enableBlockV8OptimizerOnUnfamiliarSites'));

    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    page = createPage(SafeBrowsingSetting.STANDARD);
    return flushTasks();
  });

  test('CheckRadioButtons', function() {
    assertFalse(!!queryBlockOnUnfamiliarSitesRadioButton(page));
  });
});

suite('V8Page_BlockOnUnfamiliarSitesFeatureEnabled', function() {
  let page: V8PageElement;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    assertTrue(
        loadTimeData.getBoolean('enableBlockV8OptimizerOnUnfamiliarSites'));
  });

  test('CheckRadioButtons_SafeBrowsingEnabled', async function() {
    page = createPage(SafeBrowsingSetting.STANDARD);
    await flushTasks();
    const radioButton = queryBlockOnUnfamiliarSitesRadioButton(page);
    assertTrue(!!radioButton);
    assertTrue(isVisible(radioButton));
    assertFalse(radioButton.disabled);
  });

  test('CheckRadioButtons_SafeBrowsingDisabled', async function() {
    page = createPage(SafeBrowsingSetting.DISABLED);
    await flushTasks();
    const radioButton = queryBlockOnUnfamiliarSitesRadioButton(page);
    assertTrue(!!radioButton);
    assertTrue(isVisible(radioButton));
    assertTrue(radioButton.disabled);
  });
});
