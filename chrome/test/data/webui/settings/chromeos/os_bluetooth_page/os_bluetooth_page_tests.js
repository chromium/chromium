// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';

import {OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {BluetoothSystemState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('OsBluetoothPageTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothPageElement|undefined} */
  let bluetoothPage;

  /** @type {?OsBluetoothDevicesSubpageBrowserProxy} */
  let browserProxy = null;

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);

    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
    bluetoothPage = document.createElement('os-settings-bluetooth-page');
    document.body.appendChild(bluetoothPage);
    flush();
  });

  test('Show bluetooth pairing UI', async function() {
    const getBluetoothPairingUi = () => bluetoothPage.shadowRoot.querySelector(
        'os-settings-bluetooth-pairing-dialog');
    const bluetoothSummary =
        bluetoothPage.shadowRoot.querySelector('os-settings-bluetooth-summary');

    const getPairNewDevice = () =>
        bluetoothPage.shadowRoot.querySelector('#pairNewDevice');

    assertTrue(!!bluetoothSummary);
    assertFalse(!!getBluetoothPairingUi());
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());

    bluetoothSummary.dispatchEvent(new CustomEvent('start-pairing'));

    await flushAsync();
    assertTrue(!!getBluetoothPairingUi());
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');

    getBluetoothPairingUi().dispatchEvent(new CustomEvent('close'));

    await flushAsync();
    assertFalse(!!getBluetoothPairingUi());

    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES, null);

    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await flushAsync();
    assertTrue(!!getPairNewDevice());

    // Simulate Bluetooth disabled
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushAsync();
    assertFalse(!!getPairNewDevice());

    // Simulate Bluetooth unavailable
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertFalse(!!getPairNewDevice());

    // Simulate Bluetooth enabled
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabling);
    await flushAsync();
    assertTrue(!!getPairNewDevice());
    getPairNewDevice().click();

    await flushAsync();
    assertTrue(!!getBluetoothPairingUi());
  });
});
