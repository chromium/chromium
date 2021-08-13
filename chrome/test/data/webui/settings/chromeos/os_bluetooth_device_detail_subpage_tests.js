// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {Router, Route, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {eventToPromise} from 'chrome://test/test_util.m.js';
// #import {assertTrue, assertEquals} from '../../../chai_assert.js';
// #import {createDefaultBluetoothDevice, FakeBluetoothConfig} from './fake_bluetooth_config.m.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// clang-format on

suite('OsBluetoothDeviceDetailPageTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothDeviceDetailSubpageElement|undefined} */
  let bluetoothDeviceDetailPage;

  /** @type {!chromeos.bluetoothConfig.mojom} */
  let mojom;

  /**
   * @type {!chromeos.bluetoothConfig.mojom.SystemPropertiesObserverInterface}
   */
  let propertiesObserver;

  setup(function() {
    mojom = chromeos.bluetoothConfig.mojom;
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  function init() {
    bluetoothDeviceDetailPage =
        document.createElement('os-settings-bluetooth-device-detail-subpage');
    document.body.appendChild(bluetoothDeviceDetailPage);
    Polymer.dom.flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override
       * @param {!chromeos.bluetoothConfig.mojom.BluetoothSystemProperties}
       *     properties
       */
      onPropertiesUpdated(properties) {
        bluetoothDeviceDetailPage.systemProperties = properties;
      },
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
  }

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise((resolve) => setTimeout(resolve));
  }

  teardown(function() {
    bluetoothDeviceDetailPage.remove();
    bluetoothDeviceDetailPage = null;
    settings.Router.getInstance().resetRouteForTesting();
  });


  test('Base Test', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*nickname=*/ 'device1');

    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushAsync();

    const params = new URLSearchParams();
    params.append('id', '12//345&6789');
    settings.Router.getInstance().navigateTo(
        settings.routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
    assertEquals('device1', bluetoothDeviceDetailPage.parentNode.pageTitle);
  });

  test('Device becomes unavailable while viewing page.', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const windowPopstatePromise = test_util.eventToPromise('popstate', window);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12345/6789&',
        /*publicName=*/ 'BeatsX',
        /*connected=*/ true,
        /*nickname=*/ 'device1');

    const device2 = createDefaultBluetoothDevice(
        /*id=*/ '987654321',
        /*publicName=*/ 'MX 3',
        /*connected=*/ true);

    bluetoothConfig.appendToPairedDeviceList([device1, device2]);
    await flushAsync();

    const params = new URLSearchParams();
    params.append('id', '12345/6789&');
    settings.Router.getInstance().navigateTo(
        settings.routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
    assertEquals('device1', bluetoothDeviceDetailPage.parentNode.pageTitle);
    bluetoothConfig.removePairedDevice(device1);
    await windowPopstatePromise;
  });
});
