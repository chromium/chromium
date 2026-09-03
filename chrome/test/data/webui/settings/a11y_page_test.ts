// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

// clang-format off
// <if expr="is_win or is_linux or is_macosx">
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsAxAnnotationsSectionElement} from 'chrome://settings/lazy_load.js';
import { assertFalse, assertTrue, assertEquals } from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
// </if>
// clang-format on

import type {AccessibilityBrowserProxy, SettingsA11yPageElement} from 'chrome://settings/lazy_load.js';
import {AccessibilityBrowserProxyImpl, getLanguageHelperInstance, LanguageHelperImpl, ToastAlertLevel} from 'chrome://settings/lazy_load.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';


class TestAccessibilityBrowserProxy extends TestBrowserProxy implements
    AccessibilityBrowserProxy {
  constructor() {
    super([
      'openTrackpadGesturesSettings',
      'recordOverscrollHistoryNavigationChanged',
      'getScreenReaderState',
    ]);
  }

  openTrackpadGesturesSettings() {
    this.methodCalled('openTrackpadGesturesSettings');
  }

  recordOverscrollHistoryNavigationChanged(enabled: boolean) {
    this.methodCalled('recordOverscrollHistoryNavigationChanged', enabled);
  }

  getScreenReaderState() {
    this.methodCalled('getScreenReaderState');
    return Promise.resolve(false);
  }
}

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    ...getFakeLanguagePrefs(),
    {
      key: 'settings.a11y.focus_highlight',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'settings.a11y.caretbrowsing.enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'settings.a11y.enable_accessibility_image_labels',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'settings.a11y.enable_ax_tree_fixing',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'settings.a11y.enable_main_node_annotations',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'settings.a11y.overscroll_history_navigation',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'settings.toast.alert_level',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: ToastAlertLevel.ALL,
    },
    {
      key: 'accessibility.captions.live_caption_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'accessibility.captions.live_caption_mask_offensive_words',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'accessibility.captions.live_caption_language',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'en-US',
    },
    {
      key: 'accessibility.captions.live_translate_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    {
      key: 'accessibility.captions.live_translate_target_language',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'en',
    },
  ];
}

suite('A11yPage', () => {
  let a11yPage: SettingsA11yPageElement;
  let browserProxy: TestAccessibilityBrowserProxy;
  let metrics: MetricsTracker;
  let prefService: PrefService;

  setup(async function() {
    loadTimeData.overrideValues({
      axTreeFixingEnabled: true,
      mainNodeAnnotationsEnabled: true,
    });

    metrics = fakeMetricsPrivate();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const initialPrefs = getInitialPrefs();
    const prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    const settingsPrefs = document.createElement('settings-prefs');
    settingsPrefs.initialize(prefsBrowserProxy.fakeApi);
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    // Set up languages helper.
    LanguageHelperImpl.resetInstanceForTesting();
    const languageHelper = getLanguageHelperInstance();
    await languageHelper.whenReady();

    // Set up test browser proxy.
    browserProxy = new TestAccessibilityBrowserProxy();
    AccessibilityBrowserProxyImpl.setInstance(browserProxy);

    a11yPage = document.createElement('settings-a11y-page');
    document.body.appendChild(a11yPage);
  });

  test('ax tree fixing toggle and pref', async () => {
    assertTrue(loadTimeData.getBoolean('axTreeFixingEnabled'));

    const toggle =
        a11yPage.shadowRoot.querySelector<SettingsToggleButtonElement>(
            '#axTreeFixing');
    assertTrue(!!toggle);

    // The AX Tree Fixing pref is off by default, so the button should be
    // toggled off.
    assertFalse(
        prefService.getPref<boolean>('settings.a11y.enable_ax_tree_fixing')
            .value);
    assertFalse(toggle.checked);

    toggle.click();
    await microtasksFinished();
    assertTrue(
        prefService.getPref<boolean>('settings.a11y.enable_ax_tree_fixing')
            .value);
    assertTrue(toggle.checked);
  });

  // <if expr="is_win or is_linux or is_macosx">
  test('check ax annotations subpage visibility', async () => {
    assertTrue(loadTimeData.getBoolean('mainNodeAnnotationsEnabled'));

    // Simulate disabling a screen reader to exclude the ax annotations subpage
    // in a DOM.
    webUIListenerCallback('screen-reader-state-changed', false);

    await microtasksFinished();
    let axAnnotationsSection =
        a11yPage.shadowRoot.querySelector<SettingsAxAnnotationsSectionElement>(
            '#AxAnnotationsSection');
    assertFalse(!!axAnnotationsSection);

    // Simulate enabling a screen reader to include the ax annotations subpage
    // in a DOM.
    webUIListenerCallback('screen-reader-state-changed', true);

    await microtasksFinished();
    axAnnotationsSection =
        a11yPage.shadowRoot.querySelector<SettingsAxAnnotationsSectionElement>(
            '#AxAnnotationsSection');
    assertTrue(!!axAnnotationsSection);
    assertTrue(isVisible(axAnnotationsSection));
  });

  test('toast toggle mapping from enum', async () => {
    const toastToggle =
        a11yPage.shadowRoot.querySelector<SettingsToggleButtonElement>(
            '#toastToggle');
    assertTrue(!!toastToggle);

    toastToggle.click();
    await microtasksFinished();
    assertEquals(
        ToastAlertLevel.ACTIONABLE,
        prefService.getPref('settings.toast.alert_level').value);
    assertFalse(toastToggle.checked);

    toastToggle.click();
    await microtasksFinished();
    assertEquals(
        ToastAlertLevel.ALL,
        prefService.getPref('settings.toast.alert_level').value);
    assertTrue(toastToggle.checked);

    // Logged metrics for both pref changes.
    assertEquals(2, metrics.count('Toast.FrequencyPrefChanged'));
  });
  // </if>

  // TODO(crbug.com/40940496): Add more test cases to improve code coverage.
});
