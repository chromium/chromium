// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {OpenWindowProxyImpl, PerformanceBrowserProxyImpl, SettingsPerformancePageElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestOpenWindowProxy} from './test_open_window_proxy.js';
import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
// clang-format on

suite('PerformancePage', function() {
  let performancePage: SettingsPerformancePageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;

  const highEfficiencyModeEnabledPref =
      'prefs.performance_tuning.high_efficiency_mode.enabled.value';

  function isToggleOn(): boolean {
    const settingsToggleButton =
        performancePage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!settingsToggleButton, 'settings-toggle-button missing');
    const crToggle =
        settingsToggleButton.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!crToggle, 'cr-toggle missing from settings-toggle-button');
    return crToggle.checked;
  }

  setup(function() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    document.body.innerHTML = '';
    performancePage = document.createElement('settings-performance-page');
    performancePage.set('prefs', {
      performance_tuning: {
        high_efficiency_mode: {
          enabled: {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
      },
    });
    document.body.appendChild(performancePage);
    flush();
  });

  test('testHighEfficiencyModeEnabled', function() {
    performancePage.set(highEfficiencyModeEnabledPref, true);
    assertTrue(isToggleOn(), 'toggle should be checked when pref is true');
  });

  test('testHighEfficiencyModeDisabled', function() {
    performancePage.set(highEfficiencyModeEnabledPref, false);
    assertFalse(
        isToggleOn(), 'toggle should not be checked when pref is false');
  });

  test('testLearnMoreLink', async function() {
    const settingsToggleButton =
        performancePage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!settingsToggleButton);
    const learnMoreLink =
        settingsToggleButton.shadowRoot!.querySelector<HTMLElement>(
            '#highEfficiencyLearnMore');
    assertTrue(!!learnMoreLink);
    learnMoreLink.click();
    const url = await openWindowProxy.whenCalled('openURL');
    assertEquals(loadTimeData.getString('highEfficiencyLearnMoreUrl'), url);
  });

  test('testSendFeedbackLink', async function() {
    const settingsToggleButton =
        performancePage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!settingsToggleButton);
    const sendFeedbackLink =
        settingsToggleButton.shadowRoot!.querySelector<HTMLElement>(
            '#highEfficiencySendFeedback');
    assertTrue(!!sendFeedbackLink);
    sendFeedbackLink.click();
    await performanceBrowserProxy.whenCalled(
        'openHighEfficiencyFeedbackDialog');
  });
});