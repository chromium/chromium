// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {Router, Route, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue, assertEquals, assertNotEquals} from '../../../chai_assert.js';
// #import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// #import {waitAfterNextRender, eventToPromise} from 'chrome://test/test_util.js';
// clang-format on

suite('OsBluetoothDevicesSubpageTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothDevicesSubpageElement|undefined} */
  let bluetoothDevicesSubpage;

  /**
   * @type {!chromeos.bluetoothConfig.mojom.SystemPropertiesObserverInterface}
   */
  let propertiesObserver;

  /** @type {!chromeos.bluetoothConfig.mojom} */
  let mojom;

  setup(function() {
    mojom = chromeos.bluetoothConfig.mojom;

    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  teardown(function() {
    bluetoothDevicesSubpage.remove();
    bluetoothDevicesSubpage = null;
    settings.Router.getInstance().resetRouteForTesting();
  });

  function init() {
    bluetoothDevicesSubpage =
        document.createElement('os-settings-bluetooth-devices-subpage');
    document.body.appendChild(bluetoothDevicesSubpage);
    Polymer.dom.flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override
       * @param {!chromeos.bluetoothConfig.mojom.BluetoothSystemProperties}
       *     properties
       */
      onPropertiesUpdated(properties) {
        bluetoothDevicesSubpage.systemProperties = properties;
      }
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
    settings.Router.getInstance().navigateTo(settings.routes.BLUETOOTH_DEVICES);
    return flushAsync();
  }

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Base Test', async function() {
    await init();
    assertTrue(!!bluetoothDevicesSubpage);
  });

  test('Toggle button creation and a11y', async function() {
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled);
    await init();
    const toggle = bluetoothDevicesSubpage.shadowRoot.querySelector(
        '#enableBluetoothToggle');
    assertTrue(toggle.checked);

    let ironAnnouncerPromise =
        test_util.eventToPromise('iron-announce', bluetoothDevicesSubpage);

    toggle.click();
    let result = await ironAnnouncerPromise;
    assertEquals(
        result.detail.text,
        bluetoothDevicesSubpage.i18n('bluetoothDisabledA11YLabel'));

    ironAnnouncerPromise =
        test_util.eventToPromise('iron-announce', bluetoothDevicesSubpage);
    toggle.click();

    result = await ironAnnouncerPromise;
    assertEquals(
        result.detail.text,
        bluetoothDevicesSubpage.i18n('bluetoothEnabledA11YLabel'));
  });

  test('Toggle button states', async function() {
    await init();

    const enableBluetoothToggle =
        bluetoothDevicesSubpage.$.enableBluetoothToggle;
    assertTrue(!!enableBluetoothToggle);

    const assertToggleEnabledState = (enabled) => {
      assertEquals(enableBluetoothToggle.checked, enabled);
      assertEquals(
          bluetoothDevicesSubpage.$.onOff.innerText,
          bluetoothDevicesSubpage.i18n(enabled ? 'deviceOn' : 'deviceOff'));
    };
    assertToggleEnabledState(/*enabled=*/ false);

    // Simulate clicking toggle.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertToggleEnabledState(/*enabled=*/ true);

    // Mock operation failing.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ false);
    await flushAsync();

    // Toggle should be off again.
    assertToggleEnabledState(/*enabled=*/ false);

    // Click again.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertToggleEnabledState(/*enabled=*/ true);

    // Mock operation success.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushAsync();

    // Toggle should still be on.
    assertToggleEnabledState(/*enabled=*/ true);

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(mojom.BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertTrue(enableBluetoothToggle.disabled);
  });

  test('Device lists states', async function() {
    await init();

    const getNoDeviceText = () =>
        bluetoothDevicesSubpage.shadowRoot.querySelector('#noDevices');

    const getDeviceList = (connected) => {
      return bluetoothDevicesSubpage.shadowRoot.querySelector(
          connected ? '#connectedDeviceList' : '#unconnectedDeviceList');
    };
    // No lists should be showing at first.
    assertFalse(!!getDeviceList(/*connected=*/ true));
    assertFalse(!!getDeviceList(/*connected=*/ false));
    assertTrue(!!getNoDeviceText());
    assertEquals(
        getNoDeviceText().textContent.trim(),
        bluetoothDevicesSubpage.i18n('bluetoothDeviceListNoConnectedDevices'));

    const connectedDevice = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);
    const unconnectedDevice = createDefaultBluetoothDevice(
        /*id=*/ '987654321', /*publicName=*/ 'MX 3',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kNotConnected);

    // Pair connected device.
    bluetoothConfig.appendToPairedDeviceList([connectedDevice]);
    await flushAsync();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(getDeviceList(/*connected=*/ true).devices.length, 1);
    assertFalse(!!getDeviceList(/*connected=*/ false));
    assertFalse(!!getNoDeviceText());

    // Pair unconnected device
    bluetoothConfig.appendToPairedDeviceList([unconnectedDevice]);
    await flushAsync();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(getDeviceList(/*connected=*/ true).devices.length, 1);
    assertTrue(!!getDeviceList(/*connected=*/ false));
    assertEquals(getDeviceList(/*connected=*/ false).devices.length, 1);
    assertFalse(!!getNoDeviceText());
  });

  test(
      'Device list items are focused on backward navigation', async function() {
        await init();

        const getDeviceList = (connected) => {
          return bluetoothDevicesSubpage.shadowRoot.querySelector(
              connected ? '#connectedDeviceList' : '#unconnectedDeviceList');
        };
        const getDeviceListItem = (connected, index) => {
          return getDeviceList(connected).shadowRoot.querySelectorAll(
              'os-settings-paired-bluetooth-list-item')[index];
        };

        const connectedDeviceId = '1';
        const connectedDevice = createDefaultBluetoothDevice(
            /*id=*/ connectedDeviceId, /*publicName=*/ 'BeatsX',
            /*connectionState=*/
            chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);
        const unconnectedDeviceId = '2';
        const unconnectedDevice = createDefaultBluetoothDevice(
            /*id=*/ unconnectedDeviceId, /*publicName=*/ 'MX 3',
            /*connectionState=*/
            chromeos.bluetoothConfig.mojom.DeviceConnectionState.kNotConnected);
        bluetoothConfig.appendToPairedDeviceList([connectedDevice]);
        bluetoothConfig.appendToPairedDeviceList([unconnectedDevice]);
        await flushAsync();

        assertTrue(!!getDeviceList(/*connected=*/ true));
        assertEquals(getDeviceList(/*connected=*/ true).devices.length, 1);
        assertTrue(!!getDeviceList(/*connected=*/ false));
        assertEquals(getDeviceList(/*connected=*/ false).devices.length, 1);

        // Simulate navigating to the detail page of |connectedDevice|.
        let params = new URLSearchParams();
        params.append('id', connectedDeviceId);
        settings.Router.getInstance().navigateTo(
            settings.routes.BLUETOOTH_DEVICE_DETAIL, params);
        await flushAsync();

        // Navigate backwards.
        assertNotEquals(
            getDeviceListItem(/*connected=*/ true, /*index=*/ 0),
            getDeviceList(/*connected=*/ true).shadowRoot.activeElement);
        let windowPopstatePromise =
            test_util.eventToPromise('popstate', window);
        settings.Router.getInstance().navigateToPreviousRoute();
        await windowPopstatePromise;

        // The first connected device list item should be focused.
        assertEquals(
            getDeviceListItem(/*connected=*/ true, /*index=*/ 0),
            getDeviceList(/*connected=*/ true).shadowRoot.activeElement);

        // Simulate navigating to the detail page of |unconnectedDevice|.
        params = new URLSearchParams();
        params.append('id', unconnectedDeviceId);
        settings.Router.getInstance().navigateTo(
            settings.routes.BLUETOOTH_DEVICE_DETAIL, params);
        await flushAsync();

        // Navigate backwards.
        assertNotEquals(
            getDeviceListItem(/*connected=*/ false, /*index=*/ 0),
            getDeviceList(/*connected=*/ false).shadowRoot.activeElement);
        windowPopstatePromise = test_util.eventToPromise('popstate', window);
        settings.Router.getInstance().navigateToPreviousRoute();
        await windowPopstatePromise;

        // The first unconnected device list item should be focused.
        assertEquals(
            getDeviceListItem(/*connected=*/ false, /*index=*/ 0),
            getDeviceList(/*connected=*/ false).shadowRoot.activeElement);
      });
});
