// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

// clang-format off
// <if expr="is_win or is_linux or is_macosx">
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {SettingsAxAnnotationsSectionElement} from 'chrome://settings/lazy_load.js';
import { assertFalse, assertTrue, assertEquals } from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// </if>
// clang-format on

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AccessibilityBrowserProxy, SettingsA11yPageElement} from 'chrome://settings/lazy_load.js';
import {AccessibilityBrowserProxyImpl, ToastAlertLevel} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {getFakeLanguagePrefs} from './fake_language_settings_private.js';


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

suite('A11yPage', () => {
  let a11yPage: SettingsA11yPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let browserProxy: TestAccessibilityBrowserProxy;
  let metrics: MetricsTracker;

  setup(function() {
    loadTimeData.overrideValues({
      axTreeFixingEnabled: true,
      mainNodeAnnotationsEnabled: true,
    });

    metrics = fakeMetricsPrivate();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsPrefs = document.createElement('settings-prefs');
    const settingsPrivate = new FakeSettingsPrivate(getFakeLanguagePrefs());
    settingsPrefs.initialize(settingsPrivate);
    document.body.appendChild(settingsPrefs);

    return CrSettingsPrefs.initialized.then(function() {
      // Set up test browser proxy.
      browserProxy = new TestAccessibilityBrowserProxy();
      AccessibilityBrowserProxyImpl.setInstance(browserProxy);

      // Set up languages helper.
      const settingsLanguages = document.createElement('settings-languages');
      settingsLanguages.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
      document.body.appendChild(settingsLanguages);

      a11yPage = document.createElement('settings-a11y-page');
      a11yPage.prefs = settingsPrefs.prefs;
      fakeDataBind(settingsPrefs, a11yPage, 'prefs');

      document.body.appendChild(a11yPage);
      flush();

      return settingsLanguages.whenReady();
    });
  });

  test('ax tree fixing toggle and pref', async () => {
    assertTrue(loadTimeData.getBoolean('axTreeFixingEnabled'));

    const toggle =
        a11yPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#axTreeFixing');
    assertTrue(!!toggle);
    await flushTasks();

    // The AX Tree Fixing pref is off by default, so the button should be
    // toggled off.
    assertFalse(a11yPage.getPref('settings.a11y.enable_ax_tree_fixing').value);
    assertFalse(toggle.checked);

    toggle.click();
    await flushTasks();
    assertTrue(a11yPage.getPref('settings.a11y.enable_ax_tree_fixing').value);
    assertTrue(toggle.checked);
  });

  // <if expr="is_win or is_linux or is_macosx">
  test('check ax annotations subpage visibility', async () => {
    assertTrue(loadTimeData.getBoolean('mainNodeAnnotationsEnabled'));

    // Simulate disabling a screen reader to exclude the ax annotations subpage
    // in a DOM.
    webUIListenerCallback('screen-reader-state-changed', false);

    await flushTasks();
    let axAnnotationsSection =
        a11yPage.shadowRoot!.querySelector<SettingsAxAnnotationsSectionElement>(
            '#AxAnnotationsSection');
    assertFalse(!!axAnnotationsSection);

    // Simulate enabling a screen reader to include the ax annotations subpage
    // in a DOM.
    webUIListenerCallback('screen-reader-state-changed', true);

    await flushTasks();
    axAnnotationsSection =
        a11yPage.shadowRoot!.querySelector<SettingsAxAnnotationsSectionElement>(
            '#AxAnnotationsSection');
    assertTrue(!!axAnnotationsSection);
    assertTrue(isVisible(axAnnotationsSection));
  });

  test('toast toggle mapping from enum', () => {
    const toastToggle =
        a11yPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#toastToggle');
    assertTrue(!!toastToggle);

    toastToggle.click();
    assertEquals(
        ToastAlertLevel.ACTIONABLE,
        a11yPage.getPref('settings.toast.alert_level').value);
    assertFalse(toastToggle.checked);

    toastToggle.click();
    assertEquals(
        ToastAlertLevel.ALL,
        a11yPage.getPref('settings.toast.alert_level').value);
    assertTrue(toastToggle.checked);

    // Logged metrics for both pref changes.
    assertEquals(2, metrics.count('Toast.FrequencyPrefChanged'));
  });
  // </if>

  // TODO(crbug.com/40940496): Add more test cases to improve code coverage.
});
