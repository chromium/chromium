// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {NetworkPredictionOptions} from 'chrome://settings/lazy_load.js';
import type {SettingsDropdownMenuElement, SpeedPageElement} from 'chrome://settings/settings.js';
import {loadTimeData, PerformanceBrowserProxyImpl, PrefsBrowserProxy, PrefService, SettingsPluralStringProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNull, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('SpeedPage', function() {
  let speedPage: SpeedPageElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  function getFakePrefs() {
    const fakePrefs = [
      {
        key: 'net.network_prediction_options',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        // By default the pref is initialized to WIFI_ONLY_DEPRECATED, but then
        // treated as STANDARD. See chrome/browser/preloading/preloading_prefs.h
        // for more details.
        value: NetworkPredictionOptions.WIFI_ONLY_DEPRECATED,
      },
      {
        key: 'cpu_performance_tier_override',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: -1,
      },
    ];
    return fakePrefs;
  }

  setup(async () => {
    prefsBrowserProxy = new TestPrefsBrowserProxy(getFakePrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);

    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    speedPage = document.createElement('settings-speed-page');
    document.body.appendChild(speedPage);
  });

  test('PreloadPagesDefault', function() {
    assertEquals(
        NetworkPredictionOptions.STANDARD,
        prefService.getPref('net.network_prediction_options').value);
    assertTrue(speedPage.$.preloadingToggle.checked);
  });

  test('PreloadPagesDisabled', async function() {
    speedPage.$.preloadingToggle.click();
    await microtasksFinished();

    assertEquals(
        NetworkPredictionOptions.DISABLED,
        prefService.getPref('net.network_prediction_options').value);
    assertFalse(speedPage.$.preloadingToggle.checked);
  });

  test('PreloadPagesStandard', async function() {
    // STANDARD is the default value, so this changes the pref to ensure that
    // clicking preloadingToggle actually updates the underlying pref.
    await prefService.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.DISABLED);

    speedPage.$.preloadingToggle.click();
    await microtasksFinished();

    assertEquals(
        NetworkPredictionOptions.STANDARD,
        prefService.getPref('net.network_prediction_options').value);
    assertTrue(speedPage.$.preloadingStandard.checked);
    assertTrue(speedPage.$.preloadingStandard.expanded);
  });

  test('PreloadPagesStandardFromExtended', async () => {
    // STANDARD is the default value, so this changes the pref to ensure that
    // clicking preloadingToggle actually updates the underlying pref.
    await prefService.setPrefValue(
        'net.network_prediction_options', NetworkPredictionOptions.EXTENDED);

    speedPage.$.preloadingStandard.click();
    await eventToPromise('change', speedPage.$.preloadingRadioGroup);
    await microtasksFinished();

    assertEquals(
        NetworkPredictionOptions.STANDARD,
        prefService.getPref('net.network_prediction_options').value);
    assertTrue(speedPage.$.preloadingStandard.checked);
    assertTrue(speedPage.$.preloadingStandard.expanded);
  });

  test('PreloadPagesExtended', async () => {
    speedPage.$.preloadingExtended.click();
    await eventToPromise('change', speedPage.$.preloadingRadioGroup);
    await microtasksFinished();

    assertEquals(
        NetworkPredictionOptions.EXTENDED,
        prefService.getPref('net.network_prediction_options').value);
    assertTrue(speedPage.$.preloadingExtended.checked);
    assertTrue(speedPage.$.preloadingExtended.expanded);
  });

  test('PreloadPagesStandardExpand', async function() {
    // By default, the preloadingStandard option will be selected and collapsed.
    assertFalse(speedPage.$.preloadingStandard.expanded);

    const expandButton = speedPage.$.preloadingStandard.$.expandButton;
    expandButton.click();
    await expandButton.updateComplete;

    assertTrue(speedPage.$.preloadingStandard.expanded);

    expandButton.click();
    await expandButton.updateComplete;

    assertFalse(speedPage.$.preloadingStandard.expanded);
  });

  test('PreloadPagesExtendedExpand', async function() {
    assertFalse(speedPage.$.preloadingExtended.expanded);

    const expandButton = speedPage.$.preloadingExtended.$.expandButton;
    expandButton.click();
    await expandButton.updateComplete;

    assertTrue(speedPage.$.preloadingExtended.expanded);

    expandButton.click();
    await expandButton.updateComplete;

    assertFalse(speedPage.$.preloadingExtended.expanded);
  });
});

suite('CpuPerformanceOverride', function() {
  let speedPage: SpeedPageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;
  let pluralStringProxy: TestPluralStringProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      cpuPerformanceEnabled: true,
    });

    pluralStringProxy = new TestPluralStringProxy();
    pluralStringProxy.text = '8 cores';
    SettingsPluralStringProxyImpl.setInstance(pluralStringProxy);

    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    performanceBrowserProxy.setCpuPerformanceInfo({
      hardwareTier: 2,
      model: 'Intel Core i7',
      cores: 8,
    });
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    const fakePrefs = [
      {
        key: 'net.network_prediction_options',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: NetworkPredictionOptions.STANDARD,
      },
      {
        key: 'cpu_performance_tier_override',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: -1,
      },
    ];
    prefsBrowserProxy = new TestPrefsBrowserProxy(fakePrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);

    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    speedPage = document.createElement('settings-speed-page');
    document.body.appendChild(speedPage);
    await performanceBrowserProxy.whenCalled('getCpuPerformanceInfo');
    await microtasksFinished();
  });

  test('HardwareInfoPresent', function() {
    const secondary = speedPage.shadowRoot.querySelector('#cpuPerformanceInfo');

    assertTrue(!!secondary);
    const text = secondary.textContent || '';
    assertStringContains(text, 'Intel Core i7');
    assertStringContains(text, '8 cores');
    assertStringContains(text, 'Tier 2: MID');
  });

  test('DropdownSelectionUpdatesPref', async function() {
    const dropdown =
        speedPage.shadowRoot.querySelector<SettingsDropdownMenuElement>(
            '#cpuPerformanceOverrideDropdown');
    assertTrue(!!dropdown);

    // Verify that the dropdown is enabled and both the dropdown and the pref
    // are initially -1.
    assertFalse(dropdown.disabled);
    assertEquals('-1', dropdown.$.dropdownMenu.value);
    assertEquals(
        -1,  // no override
        prefService.getPref('cpu_performance_tier_override').value);

    // Select 'High' (value 3).
    dropdown.$.dropdownMenu.value = '3';
    dropdown.$.dropdownMenu.dispatchEvent(new CustomEvent('change'));
    await microtasksFinished();

    // Verify that the pref changed.
    assertEquals(
        3,  // 'High'
        prefService.getPref('cpu_performance_tier_override').value);
  });

  test('DropdownDisabledWhenPolicyActive', async function() {
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'cpu_performance_tier_override',
      value: 4,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);

    const dropdown =
        speedPage.shadowRoot.querySelector<SettingsDropdownMenuElement>(
            '#cpuPerformanceOverrideDropdown');
    assertTrue(!!dropdown);

    await microtasksFinished();

    // Verify that the dropdown is disabled and shows the policy indicator.
    assertTrue(dropdown.shadowRoot.querySelector('select')!.disabled);
    assertTrue(!!dropdown.shadowRoot.querySelector('cr-policy-pref-indicator'));

    // Verify the component respects the enforced preference value.
    assertEquals(4, prefService.getPref('cpu_performance_tier_override').value);

    // Verify the UI displays the enforced value.
    assertEquals('4', dropdown.$.dropdownMenu.value);
  });
});

suite('CpuPerformanceOverrideFeatureDisabled', function() {
  let speedPage: SpeedPageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      cpuPerformanceEnabled: false,
    });

    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    const fakePrefs = [
      {
        key: 'net.network_prediction_options',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: NetworkPredictionOptions.STANDARD,
      },
    ];
    prefsBrowserProxy = new TestPrefsBrowserProxy(fakePrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);

    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    speedPage = document.createElement('settings-speed-page');
    document.body.appendChild(speedPage);
  });

  test('FeatureDisabled', function() {
    // Verify that the setting is missing.
    const section =
        speedPage.shadowRoot.querySelector('#cpuPerformanceOverrideDropdown');
    assertNull(section);
  });
});
