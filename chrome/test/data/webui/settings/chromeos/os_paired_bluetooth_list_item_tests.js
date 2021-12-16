// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertTrue, assertEquals} from '../../../chai_assert.js';
// #import {eventToPromise} from 'chrome://test/test_util.js';
// #import {createDefaultBluetoothDevice} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
// #import {Router, Route, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

suite('OsPairedBluetoothListItemTest', function() {
  /** @type {!SettingsPairedBluetoothListItemElement|undefined} */
  let pairedBluetoothListItem;

  /** @type {!chromeos.bluetoothConfig.mojom} */
  let mojom;

  setup(function() {
    mojom = chromeos.bluetoothConfig.mojom;
    pairedBluetoothListItem =
        document.createElement('os-settings-paired-bluetooth-list-item');
    document.body.appendChild(pairedBluetoothListItem);
    Polymer.dom.flush();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * @param {number} batteryPercentage
   */
  async function setBatteryPercentage(batteryPercentage) {
    pairedBluetoothListItem.device.deviceProperties.batteryInfo = {
      defaultProperties: {batteryPercentage: batteryPercentage}
    };
    pairedBluetoothListItem.device = {...pairedBluetoothListItem.device};
    return flushAsync();
  }

  test(
      'Device name, type, battery info and a11y labels', async function() {
        // Device with no nickname, battery info and unknown device type.
        const publicName = 'BeatsX';
        const device = createDefaultBluetoothDevice(
            /*id=*/ '123456789', /*publicName=*/ publicName,
            /*connectionState=*/
            chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);
        pairedBluetoothListItem.device = device;

        const itemIndex = 3;
        const listSize = 15;
        pairedBluetoothListItem.itemIndex = itemIndex;
        pairedBluetoothListItem.listSize = listSize;
        await flushAsync();

        const getDeviceName = () => {
          return pairedBluetoothListItem.$.deviceName;
        };
        const getBatteryInfo = () => {
          return pairedBluetoothListItem.shadowRoot.querySelector(
              'bluetooth-device-battery-info');
        };
        const getDeviceTypeIcon = () => {
          return pairedBluetoothListItem.shadowRoot.querySelector(
              'bluetooth-icon');
        };
        const getItemA11yLabel = () => {
          return pairedBluetoothListItem.shadowRoot.querySelector('.list-item')
              .ariaLabel;
        };
        const getSubpageButtonA11yLabel = () => {
          return pairedBluetoothListItem.$.subpageButton.ariaLabel;
        };
        assertTrue(!!getDeviceName());
        assertEquals(getDeviceName().innerText, publicName);
        assertFalse(!!getBatteryInfo());
        assertTrue(!!getDeviceTypeIcon());
        assertEquals(
            getItemA11yLabel(),
            pairedBluetoothListItem.i18n(
                'bluetoothPairedDeviceItemA11yLabelTypeUnknown', itemIndex + 1,
                listSize, publicName));

        // Set device nickname, type and battery info.
        const nickname = 'nickname';
        device.nickname = nickname;
        device.deviceProperties.deviceType =
            chromeos.bluetoothConfig.mojom.DeviceType.kComputer;
        const batteryPercentage = 60;
        device.deviceProperties.batteryInfo = {
          defaultProperties: {batteryPercentage: batteryPercentage}
        };
        pairedBluetoothListItem.device = {...device};
        await flushAsync();

        assertTrue(!!getDeviceName());
        assertEquals(getDeviceName().innerText, nickname);
        assertTrue(!!getBatteryInfo());
        assertEquals(
            getBatteryInfo().device,
            pairedBluetoothListItem.device.deviceProperties);
        assertEquals(
            getItemA11yLabel(),
            pairedBluetoothListItem.i18n(
                'bluetoothPairedDeviceItemA11yLabelTypeComputerWithBatteryInfo',
                itemIndex + 1, listSize, nickname, batteryPercentage));
      });

  test('Battery percentage out of bounds', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);
    pairedBluetoothListItem.device = device;

    const getBatteryInfo = () => {
      return pairedBluetoothListItem.shadowRoot.querySelector(
          'bluetooth-device-battery-info');
    };

    await setBatteryPercentage(-10);
    assertFalse(!!getBatteryInfo());

    await setBatteryPercentage(101);
    assertFalse(!!getBatteryInfo());
  });

  test('Selecting item routes to detail subpage', async function() {
    const id = '123456789';
    const device = createDefaultBluetoothDevice(
        id, /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);
    pairedBluetoothListItem.device = device;
    await flushAsync();

    const getItemContainer = () => {
      return pairedBluetoothListItem.shadowRoot.querySelector('.list-item');
    };
    const assertInDetailSubpage = async () => {
      await flushAsync();
      assertEquals(
          Router.getInstance().getCurrentRoute(),
          settings.routes.BLUETOOTH_DEVICE_DETAIL);
      assertEquals(
          id, settings.Router.getInstance().getQueryParameters().get('id'));

      settings.Router.getInstance().resetRouteForTesting();
      assertEquals(
          Router.getInstance().getCurrentRoute(), settings.routes.BASIC);
    };

    // Simulate clicking item.
    assertEquals(Router.getInstance().getCurrentRoute(), settings.routes.BASIC);
    getItemContainer().click();
    await assertInDetailSubpage();

    // Simulate pressing enter on the item.
    getItemContainer().dispatchEvent(
        new KeyboardEvent('keydown', {'key': 'Enter'}));
    await assertInDetailSubpage();

    // Simulate pressing space on the item.
    getItemContainer().dispatchEvent(
        new KeyboardEvent('keydown', {'key': ' '}));
    await assertInDetailSubpage();

    // Simulate clicking the item's subpage button.
    pairedBluetoothListItem.$.subpageButton.click();
    await assertInDetailSubpage();
  });

  test('Enterprise-managed icon UI state', async function() {
    const getManagedIcon = () => {
      return pairedBluetoothListItem.$$('#managedIcon');
    };
    assertFalse(!!getManagedIcon());

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        mojom.AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ mojom.DeviceType.kMouse,
        /*opt_isBlockedByPolicy=*/ true);

    pairedBluetoothListItem.device = Object.assign({}, device);
    await flushAsync();

    // The icon should now be showing.
    assertTrue(!!getManagedIcon());

    // Simulate hovering over the icon.
    const showTooltipPromise =
        eventToPromise('managed-tooltip-state-change', pairedBluetoothListItem);
    getManagedIcon().dispatchEvent(new Event('mouseenter'));

    // The managed-tooltip-state-changed event should have been fired.
    const showTooltipEvent = await showTooltipPromise;
    assertEquals(showTooltipEvent.detail.show, true);
    assertEquals(showTooltipEvent.detail.element, getManagedIcon());
    assertEquals(
        showTooltipEvent.detail.address, device.deviceProperties.address);

    // Simulate the device being unblocked by policy.
    const hideTooltipPromise =
        eventToPromise('managed-tooltip-state-change', pairedBluetoothListItem);
    const device1 = Object.assign({}, device);
    device1.deviceProperties.isBlockedByPolicy = false;
    pairedBluetoothListItem.device = Object.assign({}, device1);

    await flushAsync();

    // The icon should now be hidden.
    assertFalse(!!getManagedIcon());

    // The managed-tooltip-state-changed event should have been fired again.
    const hideTooltipEvent = await hideTooltipPromise;
    assertEquals(hideTooltipEvent.detail.show, false);
    assertEquals(hideTooltipEvent.detail.element, undefined);
    assertEquals(
        hideTooltipEvent.detail.address, device.deviceProperties.address);
  });
});
