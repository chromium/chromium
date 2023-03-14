// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {OsSyncBrowserProxyImpl} from 'chrome://os-settings/chromeos/lazy_load.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// Prefs used by settings-multidevice-wifi-sync-item.
function getOsPrefs() {
  return {
    osWifiConfigurationsRegistered: true,
    osWifiConfigurationsSynced: true,
  };
}

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
