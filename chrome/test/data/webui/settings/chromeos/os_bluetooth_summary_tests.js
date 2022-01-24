// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import 'chrome://os-settings/strings.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {Router, Route, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertTrue} from '../../../chai_assert.js';
// #import {createDefaultBluetoothDevice, FakeBluetoothConfig,} from 'chrome://test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
// #import {setBluetoothConfigForTesting} from 'chrome://resources/cr_components/chromeos/bluetooth/cros_bluetooth_config.js';
// #import {mojoString16ToString} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';
// #import {eventToPromise} from 'chrome://test/test_util.js';
// clang-format on

suite('OsBluetoothSummaryTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothSummaryElement|undefined} */
  let bluetoothSummary;

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

  function init() {
    bluetoothSummary = document.createElement('os-settings-bluetooth-summary');
    document.body.appendChild(bluetoothSummary);
    Polymer.dom.flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override
       * @param {!chromeos.bluetoothConfig.mojom.BluetoothSystemProperties}
       *     properties
       */
      onPropertiesUpdated(properties) {
        bluetoothSummary.systemProperties = properties;
      }
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
  }

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Route to Bluetooth devices subpage', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushAsync();
    const iconButton = bluetoothSummary.$$('#arrowIconButton');
    assertTrue(!!iconButton);
    iconButton.click();

    await flushAsync();
    assertEquals(
        settings.Router.getInstance().getCurrentRoute(),
        settings.routes.BLUETOOTH_DEVICES);
  });

  test('Toggle button creation', async function() {
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kEnabled);
    await flushAsync();
    init();
    assertTrue(bluetoothSummary.$$('#enableBluetoothToggle').checked);
  });

  test('Toggle button states', async function() {
    init();
    const enableBluetoothToggle = bluetoothSummary.$$('#enableBluetoothToggle');
    assertTrue(!!enableBluetoothToggle);
    assertFalse(enableBluetoothToggle.checked);

    // Simulate clicking toggle.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertTrue(enableBluetoothToggle.checked);

    // Mock operation failing.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ false);
    await flushAsync();

    // Toggle should be off again.
    assertFalse(enableBluetoothToggle.checked);

    // Click again.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertTrue(enableBluetoothToggle.checked);

    // Mock operation success.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushAsync();

    // Toggle should still be on.
    assertTrue(enableBluetoothToggle.checked);

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(
        chromeos.bluetoothConfig.mojom.BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertTrue(enableBluetoothToggle.disabled);
  });

  test('UI states test', async function() {
    init();

    // Simulate device state is disabled.
    const bluetoothSecondaryLabel =
        bluetoothSummary.$$('#bluetoothSecondaryLabel');
    const getBluetoothArrowIconBtn = () =>
        bluetoothSummary.$$('#arrowIconButton');
    const getBluetoothStatusIcon = () => bluetoothSummary.$$('#statusIcon');

    assertFalse(!!getBluetoothArrowIconBtn());
    assertTrue(!!getBluetoothStatusIcon());
    assertTrue(!!bluetoothSecondaryLabel);
    let label = bluetoothSecondaryLabel.textContent.trim();

    assertEquals(bluetoothSummary.i18n('bluetoothSummaryPageOff'), label);
    assertEquals(
        'os-settings:bluetooth-disabled', getBluetoothStatusIcon().icon);

    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushAsync();

    assertTrue(!!getBluetoothArrowIconBtn());
    // Bluetooth Icon should be default because no devices are connected.
    assertEquals('cr:bluetooth', getBluetoothStatusIcon().icon);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1');
    const device2 = createDefaultBluetoothDevice(
        /*id=*/ '987654321', /*publicName=*/ 'MX 3',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected);
    const device3 = createDefaultBluetoothDevice(
        /*id=*/ '456789', /*publicName=*/ 'Radio head',
        /*connectionState=*/
        chromeos.bluetoothConfig.mojom.DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device3');

    const mockPairedBluetoothDeviceProperties = [
      device1,
      device2,
      device3,
    ];

    // Simulate 3 connected devices.
    bluetoothConfig.appendToPairedDeviceList(
        mockPairedBluetoothDeviceProperties);
    await flushAsync();

    assertEquals(
        'os-settings:bluetooth-connected', getBluetoothStatusIcon().icon);

    label = bluetoothSecondaryLabel.textContent.trim();
    assertEquals(
        bluetoothSummary.i18n(
            'bluetoothSummaryPageTwoOrMoreDevicesDescription', device1.nickname,
            mockPairedBluetoothDeviceProperties.length - 1),
        label);

    // Simulate 2 connected devices.
    bluetoothConfig.removePairedDevice(device3);
    await flushAsync();

    label = bluetoothSecondaryLabel.textContent.trim();
    assertEquals(
        bluetoothSummary.i18n(
            'bluetoothSummaryPageTwoDevicesDescription', device1.nickname,
            mojoString16ToString(device2.deviceProperties.publicName)),
        label);

    // Simulate a single connected device.
    bluetoothConfig.removePairedDevice(device2);
    await flushAsync();

    label = bluetoothSecondaryLabel.textContent.trim();
    assertEquals(device1.nickname, label);

    /// Simulate no connected device.
    bluetoothConfig.removePairedDevice(device1);
    await flushAsync();

    label = bluetoothSecondaryLabel.textContent.trim();
    assertEquals(bluetoothSummary.i18n('bluetoothSummaryPageOn'), label);
    assertEquals('cr:bluetooth', getBluetoothStatusIcon().icon);
  });

  test('start-pairing is fired on pairNewDeviceBtn click', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushAsync();

    const toggleBluetoothPairingUiPromise =
        test_util.eventToPromise('start-pairing', bluetoothSummary);
    const getPairNewDeviceBtn = () => bluetoothSummary.$$('#pairNewDeviceBtn');

    assertTrue(!!getPairNewDeviceBtn());
    getPairNewDeviceBtn().click();

    await toggleBluetoothPairingUiPromise;
  });

  test('Secondary user', async function() {
    const primaryUserEmail = 'test@gmail.com';
    loadTimeData.overrideValues({
      isSecondaryUser: true,
      primaryUserEmail,
    });
    init();

    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushAsync();
    const bluetoothSummaryPrimary = bluetoothSummary.$$('#bluetoothSummary');
    const bluetoothSummarySecondary =
        bluetoothSummary.$$('#bluetoothSummarySeconday');
    const bluetoothSummarySecondaryText =
        bluetoothSummary.$$('#bluetoothSummarySecondayText');

    assertFalse(!!bluetoothSummaryPrimary);
    assertTrue(!!bluetoothSummarySecondary);

    assertEquals(
        bluetoothSummary.i18n(
            'bluetoothPrimaryUserControlled', primaryUserEmail),
        bluetoothSummarySecondaryText.textContent.trim());
  });

});
