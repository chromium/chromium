// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {IronCollapseElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {BATTERY_SAVER_MODE_PREF, BatterySaverModeState, ControlledRadioButtonElement, PerformanceBrowserProxyImpl, PerformanceMetricsProxyImpl, SettingsBatteryPageElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';

suite('BatteryPage', function() {
  let batteryPage: SettingsBatteryPageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;

  setup(function() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    batteryPage = document.createElement('settings-battery-page');
    batteryPage.set('prefs', {
      performance_tuning: {
        battery_saver_mode: {
          state: {
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: BatterySaverModeState.DISABLED,
          },
        },
      },
    });
    document.body.appendChild(batteryPage);
    flush();
  });

  test('testBatterySaverModeEnabledOnBattery', function() {
    batteryPage.setPrefValue(
        BATTERY_SAVER_MODE_PREF, BatterySaverModeState.ENABLED_ON_BATTERY);
    flush();
    assertTrue(
        batteryPage.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#toggleButton')!.checked,
        'toggle should be checked when battery saver mode is enabled on ' +
            'battery');
    assertTrue(
        batteryPage.shadowRoot!
            .querySelector<IronCollapseElement>('#radioGroupCollapse')!.opened,
        'collapse should be open when battery saver mode is enabled on ' +
            'battery');
    assertEquals(
        String(BatterySaverModeState.ENABLED_ON_BATTERY),
        batteryPage.shadowRoot!
            .querySelector<SettingsRadioGroupElement>('#radioGroup')!.selected,
        'selected radio button should be enabled on battery');
  });

  test('testBatterySaverModeEnabledBelowThreshold', function() {
    batteryPage.setPrefValue(
        BATTERY_SAVER_MODE_PREF, BatterySaverModeState.ENABLED_BELOW_THRESHOLD);
    flush();
    assertTrue(
        batteryPage.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#toggleButton')!.checked,
        'toggle should be checked when battery saver mode is enabled below ' +
            'threshold');
    assertTrue(
        batteryPage.shadowRoot!
            .querySelector<IronCollapseElement>('#radioGroupCollapse')!.opened,
        'collapse should be open when battery saver mode is enabled below ' +
            'threshold');
    assertEquals(
        String(BatterySaverModeState.ENABLED_BELOW_THRESHOLD),
        batteryPage.shadowRoot!
            .querySelector<SettingsRadioGroupElement>('#radioGroup')!.selected,
        'selected radio button should be enabled below threshold');
  });

  test('testBatterySaverModeDisabled', function() {
    batteryPage.setPrefValue(
        BATTERY_SAVER_MODE_PREF, BatterySaverModeState.DISABLED);
    assertFalse(
        batteryPage.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#toggleButton')!.checked,
        'toggle should be unchecked when battery saver mode is disabled');
    assertFalse(
        batteryPage.shadowRoot!
            .querySelector<IronCollapseElement>('#radioGroupCollapse')!.opened,
        'collapse should be closed when battery saver mode is disabled');
  });

  test('testBatterySaverModeMetrics', async function() {
    batteryPage.setPrefValue(
        BATTERY_SAVER_MODE_PREF, BatterySaverModeState.DISABLED);

    batteryPage.shadowRoot!
        .querySelector<SettingsToggleButtonElement>('#toggleButton')!.click();
    let state = await performanceMetricsProxy.whenCalled(
        'recordBatterySaverModeChanged');
    assertEquals(BatterySaverModeState.ENABLED_BELOW_THRESHOLD, state);

    performanceMetricsProxy.reset();
    batteryPage.shadowRoot!
        .querySelector<ControlledRadioButtonElement>(
            '#enabledOnBatteryButton')!.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordBatterySaverModeChanged');
    assertEquals(BatterySaverModeState.ENABLED_ON_BATTERY, state);

    performanceMetricsProxy.reset();
    batteryPage.shadowRoot!
        .querySelector<SettingsToggleButtonElement>('#toggleButton')!.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordBatterySaverModeChanged');
    assertEquals(BatterySaverModeState.DISABLED, state);
  });
});
