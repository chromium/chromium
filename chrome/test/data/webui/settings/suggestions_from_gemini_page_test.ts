// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsSuggestionsFromGeminiPageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, MetricsBrowserProxyImpl, OpenWindowProxyImpl, PrefsBrowserProxy, PrefService, SuggestionsFromGeminiAction} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('SuggestionsFromGeminiPage', function() {
  let openWindowProxy: TestOpenWindowProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  setup(async function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const prefsBrowserProxy = new TestPrefsBrowserProxy([
      {
        key: 'generated.find_and_fill_with_gemini',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    ]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    loadTimeData.overrideValues({
      personalContextConnectedAppsUrl: 'https://gemini.google.com/apps',
    });

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  async function setupPage():
      Promise<SettingsSuggestionsFromGeminiPageElement> {
    const page: SettingsSuggestionsFromGeminiPageElement =
        document.createElement('settings-suggestions-from-gemini-page');

    document.body.appendChild(page);
    await microtasksFinished();
    return page;
  }

  test('ManageConnectedAppsClick', async function() {
    const subpage = await setupPage();
    const row = subpage.shadowRoot.querySelector<HTMLElement>(
        '#manageConnectedAppsLinkRow');
    assertTrue(!!row);
    assertTrue(isVisible(row));

    row.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('personalContextConnectedAppsUrl'), url);

    const userAction = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'PersonalContext.Settings.ManageConnectedAppsClick', userAction);
  });

  test('ToggleChangeRecordsMetrics', async function() {
    const subpage = await setupPage();
    const toggle = subpage.shadowRoot.querySelector<HTMLElement>(
        '#suggestionsFromGeminiToggle');
    assertTrue(!!toggle);

    // Click toggle to turn OFF
    toggle.click();
    let action = await metricsBrowserProxy.whenCalled(
        'recordSuggestionsFromGeminiAction');
    assertEquals(SuggestionsFromGeminiAction.TOGGLE_OFF, action);
    let userAction = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals('PersonalContext.Settings.ToggledOff', userAction);

    metricsBrowserProxy.reset();

    // Click toggle to turn ON
    toggle.click();
    action = await metricsBrowserProxy.whenCalled(
        'recordSuggestionsFromGeminiAction');
    assertEquals(SuggestionsFromGeminiAction.TOGGLE_ON, action);
    userAction = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals('PersonalContext.Settings.ToggledOn', userAction);
  });

  test('FocusBackButton', async function() {
    const subpage = await setupPage();
    let focusCalled = false;
    const settingsSubpage =
        subpage.shadowRoot.querySelector('settings-subpage');
    assertTrue(!!settingsSubpage);
    settingsSubpage.focusBackButton = () => {
      focusCalled = true;
      return Promise.resolve();
    };
    subpage.focusBackButton();
    assertTrue(focusCalled);
  });
});
