// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsAiTabOrganizationSubpageElement, SettingsHistorySearchPageElement} from 'chrome://settings/lazy_load.js';
import {FeatureOptInState, SettingsAiPageFeaturePrefName as PrefName} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

// clang-format on

suite('TabOrganizationSubpage', function() {
  let subpage: SettingsAiTabOrganizationSubpageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-ai-tab-organization-subpage');
    subpage.prefs = settingsPrefs.prefs;
    document.body.appendChild(subpage);
    return flushTasks();
  }

  test('tabOrganizationLearnMore', async () => {
    await createPage();

    const learnMoreLink = subpage.shadowRoot!.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.href,
        loadTimeData.getString('tabOrganizationLearnMoreUrl'));
  });
});

suite('HistorySearchSubpage', function() {
  let openWindowProxy: TestOpenWindowProxy;
  let subpage: SettingsHistorySearchPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-history-search-page');
    subpage.prefs = settingsPrefs.prefs;
    document.body.appendChild(subpage);
    return flushTasks();
  }

  test('historySearchToggle', async () => {
    await createPage();

    const toggle = subpage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!toggle);

    // Check NOT_INITIALIZED case.
    assertEquals(
        FeatureOptInState.NOT_INITIALIZED,
        subpage.getPref(PrefName.HISTORY_SEARCH).value);
    assertFalse(toggle.checked);

    // Check ENABLED case.
    toggle.click();
    assertEquals(
        FeatureOptInState.ENABLED,
        subpage.getPref(PrefName.HISTORY_SEARCH).value);
    assertTrue(toggle.checked);

    // Check DISABLED case.
    toggle.click();
    assertEquals(
        FeatureOptInState.DISABLED,
        subpage.getPref(PrefName.HISTORY_SEARCH).value);
    assertFalse(toggle.checked);
  });

  test('historySearchLinkout', async function() {
    await createPage();

    const linkout = subpage.shadowRoot!.querySelector('cr-link-row');
    assertTrue(!!linkout);

    linkout.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('historySearchDataHomeUrl'));
  });

  test('historySearchLearnMore', async () => {
    await createPage();

    const learnMoreLink = subpage.shadowRoot!.querySelector('a');
    assertTrue(!!learnMoreLink);
    assertEquals(
        learnMoreLink.href,
        loadTimeData.getString('historySearchLearnMoreUrl'));
  });
});
