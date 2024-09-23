// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {OsSyncBrowserProxy, OsSyncBrowserProxyImpl, OsSyncPrefs} from 'chrome://os-settings/lazy_load.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestOsSyncBrowserProxy extends TestBrowserProxy implements
    OsSyncBrowserProxy {
  constructor() {
    super([
      'setOsSyncDatatypes',
      'sendOsSyncPrefsChanged',
    ]);
  }

  didNavigateToOsSyncPage(): void {}
  didNavigateAwayFromOsSyncPage(): void {}

  setOsSyncDatatypes(osSyncPrefs: OsSyncPrefs): void {
    this.methodCalled('setOsSyncDatatypes', osSyncPrefs);
  }

  sendOsSyncPrefsChanged(): void {
    this.methodCalled('sendOsSyncPrefsChanged');
  }
}

suite('<settings-multidevice-wifi-sync-item>', () => {
  // Prefs used by settings-multidevice-wifi-sync-item.
  const osSyncPrefs = {
    osWifiConfigurationsRegistered: true,
    osWifiConfigurationsSynced: true,
  };

  let wifiSyncItem: HTMLElement;

  setup(() => {
    const browserProxy = new TestOsSyncBrowserProxy();
    OsSyncBrowserProxyImpl.setInstanceForTesting(browserProxy);
    wifiSyncItem =
        document.createElement('settings-multidevice-wifi-sync-item');
    document.body.appendChild(wifiSyncItem);
    flush();
  });

  teardown(() => {
    wifiSyncItem.remove();
  });

  test('Wifi Sync off', async () => {
    const prefs = osSyncPrefs;
    prefs.osWifiConfigurationsSynced = false;
    webUIListenerCallback('os-sync-prefs-changed', prefs);
    flush();

    assert(wifiSyncItem.shadowRoot!.querySelector(
        'settings-multidevice-wifi-sync-disabled-link'));

    const toggle = wifiSyncItem.shadowRoot!.querySelector('cr-toggle');
    assert(toggle);
    assertTrue(toggle.disabled);
    assertFalse(toggle.checked);
  });

  test('Wifi Sync on', async () => {
    const prefs = osSyncPrefs;
    prefs.osWifiConfigurationsSynced = true;
    webUIListenerCallback('os-sync-prefs-changed', prefs);
    flush();

    assertEquals(
        null,
        wifiSyncItem.shadowRoot!.querySelector(
            'settings-multidevice-wifi-sync-disabled-link'));
  });
});
