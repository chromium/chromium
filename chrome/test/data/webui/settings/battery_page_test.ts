// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {OpenWindowProxyImpl, PerformanceBrowserProxyImpl, SettingsBatteryPageElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestOpenWindowProxy} from './test_open_window_proxy.js';
import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
// clang-format on

suite('BatteryPage', function() {
  let batteryPage: SettingsBatteryPageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;

  const batterySaverModeEnabledPref =
      'prefs.performance_tuning.battery_saver_mode.state.value';

  setup(function() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    batteryPage = document.createElement('settings-battery-page');
    batteryPage.set('prefs', {
      performance_tuning: {
        battery_saver_mode: {
          state: {
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: batteryPage.batterySaverModeStatePrefValues.disabled,
          },
        },
      },
    });
    document.body.appendChild(batteryPage);
    flush();
  });

  test('testBatterySaverModeEnabledOnBattery', function() {
    batteryPage.set(
        batterySaverModeEnabledPref,
        batteryPage.batterySaverModeStatePrefValues.enabledOnBattery);
    flush();
    assertTrue(
        batteryPage.$.toggleButton.checked,
        'toggle should be checked when battery saver mode is enabled on ' +
            'battery');
    assertTrue(
        batteryPage.$.radioGroupCollapse.opened,
        'collapse should be open when battery saver mode is enabled on ' +
            'battery');
    assertEquals(
        String(batteryPage.batterySaverModeStatePrefValues.enabledOnBattery),
        batteryPage.$.radioGroup.selected,
        'selected radio button should be enabled on battery');
  });

  test('testBatterySaverModeEnabledBelowThreshold', function() {
    batteryPage.set(
        batterySaverModeEnabledPref,
        batteryPage.batterySaverModeStatePrefValues.enabledBelowThreshold);
    flush();
    assertTrue(
        batteryPage.$.toggleButton.checked,
        'toggle should be checked when battery saver mode is enabled below ' +
            'threshold');
    assertTrue(
        batteryPage.$.radioGroupCollapse.opened,
        'collapse should be open when battery saver mode is enabled below ' +
            'threshold');
    assertEquals(
        String(
            batteryPage.batterySaverModeStatePrefValues.enabledBelowThreshold),
        batteryPage.$.radioGroup.selected,
        'selected radio button should be enabled below threshold');
  });

  test('testBatterySaverModeDisabled', function() {
    batteryPage.set(
        batterySaverModeEnabledPref,
        batteryPage.batterySaverModeStatePrefValues.disabled);
    assertFalse(
        batteryPage.$.toggleButton.checked,
        'toggle should be unchecked when battery saver mode is disabled');
    assertFalse(
        batteryPage.$.radioGroupCollapse.opened,
        'collapse should be closed when battery saver mode is disabled');
  });

  test('testLearnMoreLink', async function() {
    const learnMoreLink =
        batteryPage.$.toggleButton.shadowRoot!.querySelector<HTMLElement>(
            '#batterySaverLearnMore');
    assertTrue(!!learnMoreLink);
    learnMoreLink.click();
    const url = await openWindowProxy.whenCalled('openURL');
    assertEquals(loadTimeData.getString('batterySaverLearnMoreUrl'), url);
  });

  test('testSendFeedbackLink', async function() {
    const sendFeedbackLink =
        batteryPage.$.toggleButton.shadowRoot!.querySelector<HTMLElement>(
            '#batterySaverSendFeedback');

    // <if expr="_google_chrome">
    assertTrue(!!sendFeedbackLink);
    sendFeedbackLink.click();
    await performanceBrowserProxy.whenCalled('openBatterySaverFeedbackDialog');
    // </if>

    // <if expr="not _google_chrome">
    assertFalse(!!sendFeedbackLink);
    // </if>
  });
});
