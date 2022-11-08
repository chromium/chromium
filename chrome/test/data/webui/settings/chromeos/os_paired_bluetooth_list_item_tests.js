// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {AudioOutputCapability, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createDefaultBluetoothDevice} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('OsPairedBluetoothListItemTest', function() {
  /** @type {!SettingsPairedBluetoothListItemElement|undefined} */
  let pairedBluetoothListItem;

  setup(function() {
    pairedBluetoothListItem =
        document.createElement('os-settings-paired-bluetooth-list-item');
    document.body.appendChild(pairedBluetoothListItem);
    flush();
  });

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * @param {number} batteryPercentage
   */
  async function setBatteryPercentage(batteryPercentage) {
    pairedBluetoothListItem.device.deviceProperties.batteryInfo = {
      defaultProperties: {batteryPercentage: batteryPercentage},
    };
    pairedBluetoothListItem.device = {...pairedBluetoothListItem.device};
    return flushAsync();
  }

  test(
      'Device name, type, connection state, battery info and a11y labels',
      async function() {
        // Device with no nickname, battery info, not connected and unknown
        // device type.
        const publicName = 'BeatsX';
        const device = createDefaultBluetoothDevice(
            /*id=*/ '123456789', /*publicName=*/ publicName,
            /*connectionState=*/
            DeviceConnectionState.kNotConnected);
        pairedBluetoothListItem.device = device;

        const itemIndex = 3;
        const listSize = 15;
        pairedBluetoothListItem.itemIndex = itemIndex;
        pairedBluetoothListItem.listSize = listSize;
        await flushAsync();

        const getDeviceName = () => {
          return pairedBluetoothListItem.$.deviceName;
        };
        const isShowingSubtitle = () => {
          return !pairedBluetoothListItem.$.subtitle.hidden;
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
        assertFalse(!!isShowingSubtitle());
        assertFalse(!!getBatteryInfo());
        assertTrue(!!getDeviceTypeIcon());

        let expectedA11yLabel = pairedBluetoothListItem.i18n(
                                    'bluetoothA11yDeviceName', itemIndex + 1,
                                    listSize, publicName) +
            ' ' +
            pairedBluetoothListItem.i18n(
                'bluetoothA11yDeviceConnectionStateNotConnected') +
            ' ' +
            pairedBluetoothListItem.i18n('bluetoothA11yDeviceTypeUnknown');
        assertEquals(getItemA11yLabel(), expectedA11yLabel);

        // Set device to connecting.
        device.deviceProperties.connectionState =
            DeviceConnectionState.kConnecting;
        pairedBluetoothListItem.device = {...device};
        await flushAsync();

        assertTrue(!!getDeviceName());
        assertEquals(getDeviceName().innerText, publicName);
        assertTrue(!!isShowingSubtitle());
        assertFalse(!!getBatteryInfo());
        assertTrue(!!getDeviceTypeIcon());

        expectedA11yLabel = pairedBluetoothListItem.i18n(
                                'bluetoothA11yDeviceName', itemIndex + 1,
                                listSize, publicName) +
            ' ' +
            pairedBluetoothListItem.i18n(
                'bluetoothA11yDeviceConnectionStateConnecting') +
            ' ' +
            pairedBluetoothListItem.i18n('bluetoothA11yDeviceTypeUnknown');
        assertEquals(getItemA11yLabel(), expectedA11yLabel);

        // Set device nickname, connection state, type and battery info.
        const nickname = 'nickname';
        device.nickname = nickname;
        device.deviceProperties.connectionState =
            DeviceConnectionState.kConnected;
        device.deviceProperties.deviceType = DeviceType.kComputer;
        const batteryPercentage = 60;
        device.deviceProperties.batteryInfo = {
          defaultProperties: {batteryPercentage: batteryPercentage},
        };
        pairedBluetoothListItem.device = {...device};
        await flushAsync();

        let expectedA11yBatteryLabel = pairedBluetoothListItem.i18n(
            'bluetoothA11yDeviceBatteryInfo', batteryPercentage);

        assertTrue(!!getDeviceName());
        assertEquals(getDeviceName().innerText, nickname);
        assertFalse(!!isShowingSubtitle());
        assertTrue(!!getBatteryInfo());
        assertEquals(
            getBatteryInfo().device,
            pairedBluetoothListItem.device.deviceProperties);

        expectedA11yLabel =
            pairedBluetoothListItem.i18n(
                'bluetoothA11yDeviceName', itemIndex + 1, listSize, nickname) +
            ' ' +
            pairedBluetoothListItem.i18n(
                'bluetoothA11yDeviceConnectionStateConnected') +
            ' ' +
            pairedBluetoothListItem.i18n('bluetoothA11yDeviceTypeComputer');
        assertEquals(
            getItemA11yLabel(),
            expectedA11yLabel + ' ' + expectedA11yBatteryLabel);

        // Add True Wireless battery information and make sure that it takes
        // precedence over default battery information.
        const leftBudBatteryPercentage = 19;
        const caseBatteryPercentage = 29;
        const rightBudBatteryPercentage = 39;
        device.deviceProperties.batteryInfo.leftBudInfo = {
          batteryPercentage: leftBudBatteryPercentage,
        };
        device.deviceProperties.batteryInfo.caseInfo = {
          batteryPercentage: caseBatteryPercentage,
        };
        device.deviceProperties.batteryInfo.rightBudInfo = {
          batteryPercentage: rightBudBatteryPercentage,
        };
        pairedBluetoothListItem.device = {...device};
        await flushAsync();

        expectedA11yBatteryLabel =
            pairedBluetoothListItem.i18n(
                'bluetoothA11yDeviceNamedBatteryInfoLeftBud',
                leftBudBatteryPercentage) +
            ' ' +
            pairedBluetoothListItem.i18n(
                'bluetoothA11yDeviceNamedBatteryInfoCase',
                caseBatteryPercentage) +
            ' ' +
            pairedBluetoothListItem.i18n(
                'bluetoothA11yDeviceNamedBatteryInfoRightBud',
                rightBudBatteryPercentage);

        assertEquals(
            getItemA11yLabel(),
            expectedA11yLabel + ' ' + expectedA11yBatteryLabel);
      });

  test('Battery percentage out of bounds', async function() {
    const device = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected);
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
        DeviceConnectionState.kConnected);
    pairedBluetoothListItem.device = device;
    await flushAsync();

    const getItemContainer = () => {
      return pairedBluetoothListItem.shadowRoot.querySelector('.list-item');
    };
    const assertInDetailSubpage = async () => {
      await flushAsync();
      assertEquals(
          Router.getInstance().getCurrentRoute(),
          routes.BLUETOOTH_DEVICE_DETAIL);
      assertEquals(id, Router.getInstance().getQueryParameters().get('id'));

      Router.getInstance().resetRouteForTesting();
      assertEquals(Router.getInstance().getCurrentRoute(), routes.BASIC);
    };

    // Simulate clicking item.
    assertEquals(Router.getInstance().getCurrentRoute(), routes.BASIC);
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
      return pairedBluetoothListItem.shadowRoot.querySelector('#managedIcon');
    };
    assertFalse(!!getManagedIcon());

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse,
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
