// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OsSyncBrowserProxyImpl, SyncBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';

import {TestSyncBrowserProxy} from './test_os_sync_browser_proxy.js';

// Prefs used by settings-multidevice-wifi-sync-item if
// SyncSettingsCategorization is disabled.
function getPrefs() {
  return {
    wifiConfigurationsRegistered: true,
    wifiConfigurationsSynced: true,
  };
}

// Prefs used by settings-multidevice-wifi-sync-item if
// SyncSettingsCategorization is enabled.
function getOsPrefs() {
  return {
    osWifiConfigurationsRegistered: true,
    osWifiConfigurationsSynced: true,
  };
}

suite('Multidevice_WifiSyncItem_CategorizationDisabled', function() {
  let wifiSyncItem;

  setup(function() {
    const browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      syncSettingsCategorizationEnabled: false,
    });

    wifiSyncItem =
        document.createElement('settings-multidevice-wifi-sync-item');
    document.body.appendChild(wifiSyncItem);
    flush();
  });

  teardown(function() {
    wifiSyncItem.remove();
  });

  test('Chrome Sync off', async () => {
    const prefs = getPrefs();
    prefs.wifiConfigurationsSynced = false;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    assertTrue(!!wifiSyncItem.shadowRoot.querySelector(
        'settings-multidevice-wifi-sync-disabled-link'));

    const toggle = wifiSyncItem.shadowRoot.querySelector('cr-toggle');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);
    assertFalse(toggle.checked);
  });

  test('Chrome Sync on', async () => {
    const prefs = getPrefs();
    prefs.wifiConfigurationsSynced = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();

    assertFalse(!!wifiSyncItem.shadowRoot.querySelector(
        'settings-multidevice-wifi-sync-disabled-link'));
  });
});

class TestOsSyncBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'sendOsSyncPrefsChanged',
    ]);
  }

  /** @override */
  sendOsSyncPrefsChanged() {
    this.methodCalled('sendOsSyncPrefsChanged');
  }
}

suite('Multidevice_WifiSyncItem_CategorizationEnabled', function() {
  let wifiSyncItem;

  setup(function() {
    const browserProxy = new TestOsSyncBrowserProxy();
    OsSyncBrowserProxyImpl.setInstanceForTesting(browserProxy);

    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      syncSettingsCategorizationEnabled: true,
    });

    wifiSyncItem =
        document.createElement('settings-multidevice-wifi-sync-item');
    document.body.appendChild(wifiSyncItem);
    flush();
  });

  teardown(function() {
    wifiSyncItem.remove();
  });

  test('Wifi Sync off', async () => {
    const prefs = getOsPrefs();
    prefs.osWifiConfigurationsSynced = false;
    webUIListenerCallback('os-sync-prefs-changed', prefs);
    flush();

    assertTrue(!!wifiSyncItem.shadowRoot.querySelector(
        'settings-multidevice-wifi-sync-disabled-link'));

    const toggle = wifiSyncItem.shadowRoot.querySelector('cr-toggle');
    assertTrue(!!toggle);
    assertTrue(toggle.disabled);
    assertFalse(toggle.checked);
  });

  test('Wifi Sync on', async () => {
    const prefs = getOsPrefs();
    prefs.osWifiConfigurationsSynced = true;
    webUIListenerCallback('os-sync-prefs-changed', prefs);
    flush();

    assertFalse(!!wifiSyncItem.shadowRoot.querySelector(
        'settings-multidevice-wifi-sync-disabled-link'));
  });
});
