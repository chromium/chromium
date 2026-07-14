// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import type {SettingsSystemPageElement, SystemPageBrowserProxy} from 'chrome://settings/lazy_load.js';
import {SystemPageBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {LifetimeBrowserProxyImpl, PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';

// <if expr="_google_chrome and is_win">
import {MetricsBrowserProxyImpl} from 'chrome://settings/settings.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// </if>
// <if expr="is_win">
import {loadTimeData} from 'chrome://settings/settings.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
// </if>

// clang-format on

const HARDWARE_ACCELERATION_AT_STARTUP: boolean = true;

class TestSystemPageBrowserProxy extends TestBrowserProxy implements
    SystemPageBrowserProxy {
  constructor() {
    super(['showProxySettings']);
  }

  showProxySettings() {
    this.methodCalled('showProxySettings');
  }

  wasHardwareAccelerationEnabledAtStartup() {
    return HARDWARE_ACCELERATION_AT_STARTUP;
  }
}

suite('settings system page', function() {
  let systemBrowserProxy: TestSystemPageBrowserProxy;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  // <if expr="_google_chrome and is_win">
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  // </if>
  let systemPage: SettingsSystemPageElement;
  let fakeSettingsPrivate: FakeSettingsPrivate;

  function getInitialPrefs(_isolationEnabledAtStartup: boolean):
      chrome.settingsPrivate.PrefObject[] {
    const prefs = [
      {
        key: 'background_mode.enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: 'hardware_acceleration_mode.enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: HARDWARE_ACCELERATION_AT_STARTUP,
      },
      {
        key: 'proxy',
        type: chrome.settingsPrivate.PrefType.DICTIONARY,
        value: {mode: 'system'},
      },
      {
        key: 'proxy_override_rules',
        type: chrome.settingsPrivate.PrefType.LIST,
        value: [],
      },
    ];
    // <if expr="is_win">
    prefs.push({
      key: 'isolation_state.enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: _isolationEnabledAtStartup,
    });
    // </if>
    // <if expr="_google_chrome and is_win">
    prefs.push({
      key: 'feature_notifications_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    });
    // </if>
    return prefs;
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // <if expr="is_win">
    loadTimeData.overrideValues({
      showProcessIsolationSetting: true,
    });
    // </if>

    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);
    // <if expr="_google_chrome and is_win">
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    // </if>
    systemBrowserProxy = new TestSystemPageBrowserProxy();
    SystemPageBrowserProxyImpl.setInstance(systemBrowserProxy);

    const prefsBrowserProxy = new TestPrefsBrowserProxy(
        getInitialPrefs(/*_isolationEnabledAtStartup=*/ false));
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    fakeSettingsPrivate = prefsBrowserProxy.fakeApi;
    PrefService.resetInstanceForTesting();

    await PrefService.getInstance().whenInitialized();

    systemPage = document.createElement('settings-system-page');
    document.body.appendChild(systemPage);

    // Ensure that dynamic Polymer nodes (i.e., featureNotificationsEnabled,
    // which is behind a `dom-if` block) are loaded.
    await flushTasks();
  });

  teardown(function() {
    systemPage.remove();
  });

  test('restart button', function() {
    const control = systemPage.$.hardwareAcceleration;
    assertEquals(HARDWARE_ACCELERATION_AT_STARTUP, control.checked);

    // Restart button should be hidden by default.
    assertFalse(!!control.querySelector('cr-button'));

    PrefService.getInstance().setPrefValue(
        'hardware_acceleration_mode.enabled',
        !HARDWARE_ACCELERATION_AT_STARTUP);
    flush();
    assertNotEquals(HARDWARE_ACCELERATION_AT_STARTUP, control.checked);

    const restart = control.querySelector('cr-button');
    assertTrue(!!restart);  // The "RESTART" button should be showing now.

    restart.click();
    return lifetimeBrowserProxy.whenCalled('restart');
  });

  // <if expr="is_win">
  test('process isolation restart button', function() {
    // Toggle is behind a `dom-if`, so retrieve it via `querySelector`.
    const control =
        systemPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#isolationState');
    assertTrue(!!control);
    assertFalse(control.checked);

    // Restart button should be hidden by default.
    assertFalse(!!control.querySelector('cr-button'));

    PrefService.getInstance().setPrefValue('isolation_state.enabled', true);
    flush();
    assertTrue(control.checked);

    const restart = control.querySelector('cr-button');
    assertTrue(!!restart);

    restart.click();
    return lifetimeBrowserProxy.whenCalled('restart');
  });

  test('process isolation restart button (starts enabled)', async function() {
    // Recreate the page with the pref starting as true.
    systemPage.remove();
    const prefsBrowserProxy = new TestPrefsBrowserProxy(
        getInitialPrefs(/*_isolationEnabledAtStartup=*/ true));
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    fakeSettingsPrivate = prefsBrowserProxy.fakeApi;
    PrefService.resetInstanceForTesting();

    await PrefService.getInstance().whenInitialized();

    systemPage = document.createElement('settings-system-page');
    document.body.appendChild(systemPage);
    await flushTasks();

    const control =
        systemPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#isolationState');
    assertTrue(!!control);

    // The pref starts as true.
    assertTrue(control.checked);

    // Restart button should be hidden by default because startup state matches.
    assertFalse(!!control.querySelector('cr-button'));

    // Toggle the setting off.
    PrefService.getInstance().setPrefValue('isolation_state.enabled', false);
    flush();
    assertFalse(control.checked);

    // Now the restart button should be showing.
    const restart = control.querySelector('cr-button');
    assertTrue(!!restart);

    restart.click();
    return lifetimeBrowserProxy.whenCalled('restart');
  });
  // </if>

  test('proxy row', function() {
    systemPage.$.proxy.click();
    return systemBrowserProxy.whenCalled('showProxySettings');
  });

  test('proxy row enforcement', function() {
    const control = systemPage.$.proxy;
    const showProxyButton = control.querySelector('cr-icon-button')!;
    assertTrue(control.hasAttribute('actionable'));
    assertEquals(null, control.querySelector('cr-policy-pref-indicator'));
    assertTrue(isVisible(showProxyButton));

    fakeSettingsPrivate.sendPrefChanges([{
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      extensionId: 'blah',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    flush();

    // When managed by extensions, we disable the ability to show proxy
    // settings.
    assertFalse(control.hasAttribute('actionable'));
    assertEquals(null, control.querySelector('cr-policy-pref-indicator'));
    assertFalse(isVisible(showProxyButton));

    fakeSettingsPrivate.sendPrefChanges([{
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    flush();

    // When managed by policy directly, we disable the ability to show proxy
    // settings.
    assertFalse(control.hasAttribute('actionable'));
    assertNotEquals(null, control.querySelector('cr-policy-pref-indicator'));
    assertFalse(isVisible(showProxyButton));
  });

  test('proxy row multiple sources', function() {
    const control = systemPage.$.proxyMultipleSources;
    const deviceSettings =
        control.querySelector<HTMLElement>('#proxyDeviceSettings')!;
    assertTrue(deviceSettings.hasAttribute('actionable'));
    assertFalse(isVisible(control));

    const multipleSourcesLabel = systemPage.$.proxy.querySelector<HTMLElement>(
        '#proxyMultipleSourcesLabel')!;
    assertFalse(isVisible(multipleSourcesLabel));

    // Case 1: ProxyOverrideRules is set by policy, proxy is not set (using
    // system default). Multiple sources should be shown.
    fakeSettingsPrivate.sendPrefChanges([{
      key: 'proxy_override_rules',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: [{
        'DestinationMatchers': ['https://app1.com', 'https://app2.com'],
        'ProxyList': ['HTTPS proxy.app:443'],
      }],
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    flush();

    assertTrue(deviceSettings.hasAttribute('actionable'));
    assertTrue(isVisible(control));
    assertTrue(isVisible(multipleSourcesLabel));

    // Case 2: Both ProxyOverrideRules and proxy are set by policy.
    // A single combined sources should be shown.
    fakeSettingsPrivate.sendPrefChanges([{
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    flush();

    assertFalse(deviceSettings.hasAttribute('actionable'));
    assertFalse(isVisible(control));
    assertFalse(isVisible(multipleSourcesLabel));

    // Case 3: ProxyOverrideRules is set by policy, proxy is set by an
    // extension. Multiple sources should be shown.
    fakeSettingsPrivate.sendPrefChanges([{
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      extensionId: 'extension-id-1',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    flush();

    assertFalse(deviceSettings.hasAttribute('actionable'));
    assertTrue(isVisible(control));
    assertTrue(isVisible(multipleSourcesLabel));

    // Case 4: ProxyOverrideRules and proxy are set by the same extension.
    // A single combined sources should be shown.
    fakeSettingsPrivate.sendPrefChanges([{
      key: 'proxy_override_rules',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: [{
        'DestinationMatchers': ['https://app1.com'],
        'ProxyList': ['HTTPS proxy.app:443'],
      }],
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      extensionId: 'extension-id-1',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    flush();

    assertFalse(deviceSettings.hasAttribute('actionable'));
    assertFalse(isVisible(control));
    assertFalse(isVisible(multipleSourcesLabel));

    // Case 5: ProxyOverrideRules and proxy are set by different extension.
    // Multiple sources should be shown.
    fakeSettingsPrivate.sendPrefChanges([{
      key: 'proxy',
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
      value: {mode: 'system'},
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      extensionId: 'extension-id-2',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    flush();

    assertFalse(deviceSettings.hasAttribute('actionable'));
    assertTrue(isVisible(control));
    assertTrue(isVisible(multipleSourcesLabel));
  });

  // <if expr="_google_chrome and is_win">
  test('feature notifications changed', async function() {
    function getPrefValue(): boolean {
      return PrefService.getInstance()
          .getPref<boolean>('feature_notifications_enabled')
          .value;
    }

    // Toggle is behind a `dom-if`, so retrieve it via `querySelector`
    // (`systemPage.$` only contains static Polymer nodes).
    const toggle = systemPage.shadowRoot!.querySelector<HTMLElement>(
        '#featureNotificationsEnabled');
    assertTrue(!!toggle);
    assertNotEquals(
        undefined,
        PrefService.getInstance().getPref('feature_notifications_enabled'));
    assertTrue(getPrefValue());

    toggle.click();
    assertFalse(getPrefValue());
    assertFalse(await metricsBrowserProxy.whenCalled(
        'recordFeatureNotificationsChange'));

    metricsBrowserProxy.reset();

    toggle.click();
    assertTrue(getPrefValue());
    assertTrue(await metricsBrowserProxy.whenCalled(
        'recordFeatureNotificationsChange'));
  });
  // </if>
});
