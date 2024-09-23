// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsSubpageElement, SettingsBluetoothDeviceDetailSubpageElement, SettingsBluetoothTrueWirelessImagesElement} from 'chrome://os-settings/lazy_load.js';
import {CrLinkRowElement, OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {AudioOutputCapability, BluetoothSystemProperties, DeviceBatteryInfo, DeviceConnectionState, DeviceType, SystemPropertiesObserverInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://webui-test/chromeos/bluetooth/fake_bluetooth_config.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('<os-settings-bluetooth-device-detail-subpage>', () => {
  let bluetoothConfig: FakeBluetoothConfig;
  let bluetoothDeviceDetailPage: SettingsBluetoothDeviceDetailSubpageElement;
  let propertiesObserver: SystemPropertiesObserverInterface;
  let browserProxy: TestOsBluetoothDevicesSubpageBrowserProxy;

  setup(() => {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  function init(): void {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);

    bluetoothDeviceDetailPage =
        document.createElement('os-settings-bluetooth-device-detail-subpage');
    document.body.appendChild(bluetoothDeviceDetailPage);
    flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override properties
       */
      onPropertiesUpdated(properties: BluetoothSystemProperties) {
        bluetoothDeviceDetailPage.systemProperties = properties;
      },
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
    flush();
  }

  teardown(() => {
    bluetoothDeviceDetailPage.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  function getBluetoothForgetBtn(): HTMLButtonElement|null {
    return bluetoothDeviceDetailPage.shadowRoot!
        .querySelector<HTMLButtonElement>('#forgetBtn');
  }

  function getBluetoothConnectDisconnectBtn(): HTMLButtonElement | null {
    return bluetoothDeviceDetailPage.shadowRoot!
        .querySelector<HTMLButtonElement>('#connectDisconnectBtn');
  }

  function navigateToDeviceDetailPage(id: string): void {
    const params = new URLSearchParams();
    params.append('id', id);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
  }

  function getConnectionFailedText(): HTMLElement|null {
    return bluetoothDeviceDetailPage.shadowRoot!.querySelector(
        '#connectionFailed');
  }

  function getTrueWirelessImages():
    SettingsBluetoothTrueWirelessImagesElement | null {
    return bluetoothDeviceDetailPage.shadowRoot!.querySelector(
        '#trueWirelessImages');
  }

  function getChangeMouseSettings(): CrLinkRowElement|null {
    return bluetoothDeviceDetailPage.shadowRoot!
        .querySelector<CrLinkRowElement>('#changeMouseSettings');
  }

  function getChangeKeyboardSettings():HTMLButtonElement|null {
    return bluetoothDeviceDetailPage.shadowRoot!
        .querySelector<HTMLButtonElement>('#changeKeyboardSettings');
  }

  function getBluetoothStateText(): HTMLElement {
    const text =
        bluetoothDeviceDetailPage.shadowRoot!.querySelector<HTMLElement>(
        '#bluetoothStateText');
    assertTrue(!!text);
    return text;
  }

  function getDefaultDeviceBatteryInfo(): DeviceBatteryInfo {
    return {
        defaultProperties: undefined,
        leftBudInfo: undefined,
        rightBudInfo: undefined,
        caseInfo: undefined,
    };
  }

  test('Error text is not shown after navigating away from page', async () => {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    const id = '12345/6789&';
    const device1 = createDefaultBluetoothDevice(
        id,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kNotConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

      device1.deviceProperties.batteryInfo = {
        ...getDefaultDeviceBatteryInfo(),
      defaultProperties: {batteryPercentage: 90},
    };

    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushTasks();

    navigateToDeviceDetailPage(id);
    await flushTasks();

    // Try to connect.
    getBluetoothConnectDisconnectBtn()!.click();
    await flushTasks();
    bluetoothConfig.completeConnect(/*success=*/ false);
    await flushTasks();
    assertTrue(!!getConnectionFailedText());

    navigateToDeviceDetailPage(id);
    await flushTasks();
    assertNull(getConnectionFailedText());
  });

  test('Managed by enterprise icon', async () => {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const getManagedIcon = () =>
       bluetoothDeviceDetailPage.shadowRoot!.querySelector(
          '#managedIcon');

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12345/6789&',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse,
        /*opt_isBlockedByPolicy=*/ true);

    bluetoothConfig.appendToPairedDeviceList([device]);
    await flushTasks();

    navigateToDeviceDetailPage('12345/6789&');

    await flushTasks();
    assertTrue(!!getManagedIcon());

    device.deviceProperties.isBlockedByPolicy = false;
    bluetoothConfig.updatePairedDevice(device);
    await flushTasks();
    assertNull(getManagedIcon());
  });

  test('True Wireless Images not shown when Fast pair disabled', async () => {
    loadTimeData.overrideValues({'enableFastPairFlag': false});
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12345/6789&',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kNotConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse,
        /*opt_isBlockedByPolicy=*/ true);
    const fakeUrl = {url: 'fake_image'};

    device.deviceProperties.imageInfo = {
      trueWirelessImages: {
        leftBudImageUrl: fakeUrl,
        caseImageUrl: fakeUrl,
        rightBudImageUrl: fakeUrl,
      },
      defaultImageUrl: {url: ''},
    };
      device.deviceProperties.batteryInfo = {
        ...getDefaultDeviceBatteryInfo(),
      leftBudInfo: {batteryPercentage: 90},
    };

    bluetoothConfig.appendToPairedDeviceList([device]);
    await flushTasks();

    navigateToDeviceDetailPage('12345/6789&');

    // Since Fast Pair flag is false, we don't show the component.
    await flushTasks();
    assertNull(getTrueWirelessImages());
  });

  test('True Wireless Images shown when expected', async () => {
    loadTimeData.overrideValues({'enableFastPairFlag': true});
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const device = createDefaultBluetoothDevice(
        /*id=*/ '12345/6789&',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kNotConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse,
        /*opt_isBlockedByPolicy=*/ true);
    const fakeUrl = {url: 'fake_image'};
    // Emulate missing the right bud image.
    device.deviceProperties.imageInfo = {
      trueWirelessImages: {
        leftBudImageUrl: fakeUrl,
        rightBudImageUrl: {url: ''},
        caseImageUrl: fakeUrl,
      },
      defaultImageUrl: {url: ''},
    };
      device.deviceProperties.batteryInfo = {
        ...getDefaultDeviceBatteryInfo(),
      leftBudInfo: {batteryPercentage: 90},
    };

    bluetoothConfig.appendToPairedDeviceList([device]);
    await flushTasks();

    navigateToDeviceDetailPage('12345/6789&');

    // Don't display component unless either the default image is
    // present OR all of the true wireless images are present.
    await flushTasks();
    assertNull(getTrueWirelessImages());

    // Try again with all 3 True Wireless images.
    device.deviceProperties.imageInfo!.trueWirelessImages!.rightBudImageUrl =
        fakeUrl;
    bluetoothConfig.updatePairedDevice(device);
    await flushTasks();
    assertTrue(!!getTrueWirelessImages());

    // Try again with just default image.
    device.deviceProperties.imageInfo = {
      defaultImageUrl: fakeUrl,
      trueWirelessImages: undefined,
    };
    bluetoothConfig.updatePairedDevice(device);
    await flushTasks();
    assertTrue(!!getTrueWirelessImages());

    // If battery info is not available, only show True Wireless
    // component if not connected.
    device.deviceProperties.batteryInfo = {
        ...getDefaultDeviceBatteryInfo(),
    };
    device.deviceProperties.connectionState =
        DeviceConnectionState.kNotConnected;
    bluetoothConfig.updatePairedDevice(device);
    await flushTasks();
    assertTrue(!!getTrueWirelessImages());

    device.deviceProperties.connectionState = DeviceConnectionState.kConnecting;
    bluetoothConfig.updatePairedDevice(device);
    await flushTasks();
    assertTrue(!!getTrueWirelessImages());

    device.deviceProperties.connectionState = DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device);
    await flushTasks();
    assertNull(getTrueWirelessImages());

    // Having either default battery info or True Wireless battery info
    // should show True Wireless component if device is connected.
    device.deviceProperties.batteryInfo = {
        ...getDefaultDeviceBatteryInfo(),
      defaultProperties: {batteryPercentage: 90},
    };
    bluetoothConfig.updatePairedDevice(device);
    await flushTasks();
    assertTrue(!!getTrueWirelessImages());

    device.deviceProperties.batteryInfo = {
        ...getDefaultDeviceBatteryInfo(),
      rightBudInfo: {batteryPercentage: 90},
    };
    bluetoothConfig.updatePairedDevice(device);
    await flushTasks();
    assertTrue(!!getTrueWirelessImages());
  });

  test(
      'Show change settings row, and navigate to subpages' +
      'w/ no per-device settings',
      async () => {
        loadTimeData.overrideValues({
          enableInputDeviceSettingsSplit: false,
        });

        init();
        bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

        const device1 = createDefaultBluetoothDevice(
            /*id=*/ '12//345&6789',
            /*publicName=*/ 'BeatsX',
            /*connectionState=*/
            DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device1',
            /*opt_audioCapability=*/
            AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ DeviceType.kMouse);

        bluetoothConfig.appendToPairedDeviceList([device1]);
        await flushTasks();

        navigateToDeviceDetailPage('12//345&6789');

        device1.deviceProperties.connectionState =
            DeviceConnectionState.kNotConnected;
        bluetoothConfig.updatePairedDevice(device1);
        await flushTasks();
        assertNull(getChangeMouseSettings());

        device1.deviceProperties.connectionState =
            DeviceConnectionState.kConnected;
        bluetoothConfig.updatePairedDevice(device1);
        await flushTasks();
        assertTrue(!!getChangeMouseSettings());

        getChangeMouseSettings()!.click();
        await flushTasks();

        assertEquals(routes.POINTERS, Router.getInstance().currentRoute);

        // Navigate back to the detail page.
        assertNotEquals(
            bluetoothDeviceDetailPage.shadowRoot!.activeElement,
            getChangeMouseSettings());
        let windowPopstatePromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await windowPopstatePromise;
        await waitAfterNextRender(bluetoothDeviceDetailPage);

        assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
        // Check that |#changeMouseSettings| has been focused.
        assertEquals(
            bluetoothDeviceDetailPage.shadowRoot!.activeElement,
            getChangeMouseSettings());

        device1.deviceProperties.deviceType = DeviceType.kKeyboard;
        bluetoothConfig.updatePairedDevice(device1);

        await flushTasks();
        assertNull(getChangeMouseSettings());
        assertTrue(!!getChangeKeyboardSettings());

        device1.deviceProperties.connectionState =
            DeviceConnectionState.kNotConnected;
        bluetoothConfig.updatePairedDevice(device1);
        await flushTasks();
        assertNull(getChangeKeyboardSettings());

        device1.deviceProperties.connectionState =
            DeviceConnectionState.kConnected;
        bluetoothConfig.updatePairedDevice(device1);
        await flushTasks();
        assertTrue(!!getChangeKeyboardSettings());

        getChangeKeyboardSettings()!.click();
        await flushTasks();

        assertEquals(routes.KEYBOARD, Router.getInstance().currentRoute);

        // Navigate back to the detail page.
        assertNotEquals(
            bluetoothDeviceDetailPage.shadowRoot!.activeElement,
            getChangeKeyboardSettings());
        windowPopstatePromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await windowPopstatePromise;
        await waitAfterNextRender(bluetoothDeviceDetailPage);

        // This is needed or other tests will fail.
        // TODO(gordonseto): Figure out how to remove this.
        getChangeKeyboardSettings()!.click();
        await flushTasks();
      });

  test('Show change settings row, and navigate to subpages', async () => {
    loadTimeData.overrideValues({
      enableInputDeviceSettingsSplit: true,
    });

    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12//345&6789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushTasks();

    assertNull(getChangeMouseSettings());
    assertNull(getChangeKeyboardSettings());

    navigateToDeviceDetailPage('12//345&6789');

    await flushTasks();
    assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    assertTrue(!!getChangeMouseSettings());
    assertNull(getChangeKeyboardSettings());
    assertEquals(
        bluetoothDeviceDetailPage.i18n(
            'bluetoothDeviceDetailChangeDeviceSettingsMouse'),
        getChangeMouseSettings()!.label);

    device1.deviceProperties.connectionState =
        DeviceConnectionState.kNotConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushTasks();
    assertNull(getChangeMouseSettings());

    device1.deviceProperties.connectionState = DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushTasks();
    assertTrue(!!getChangeMouseSettings());

    getChangeMouseSettings()!.click();
    await flushTasks();

    assertEquals(routes.PER_DEVICE_MOUSE, Router.getInstance().currentRoute);

    // Navigate back to the detail page.
    assertNotEquals(
        bluetoothDeviceDetailPage.shadowRoot!.activeElement,
        getChangeMouseSettings());
    let windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;
    await waitAfterNextRender(bluetoothDeviceDetailPage);

    assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    // Check that |#changeMouseSettings| has been focused.
    assertEquals(
        bluetoothDeviceDetailPage.shadowRoot!.activeElement,
        getChangeMouseSettings());

    device1.deviceProperties.deviceType = DeviceType.kKeyboard;
    bluetoothConfig.updatePairedDevice(device1);

    await flushTasks();
    assertNull(getChangeMouseSettings());
    assertTrue(!!getChangeKeyboardSettings());

    device1.deviceProperties.connectionState =
        DeviceConnectionState.kNotConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushTasks();
    assertNull(getChangeKeyboardSettings());

    device1.deviceProperties.connectionState = DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushTasks();
    assertTrue(!!getChangeKeyboardSettings());

    getChangeKeyboardSettings()!.click();
    await flushTasks();

    assertEquals(routes.PER_DEVICE_KEYBOARD, Router.getInstance().currentRoute);

    // Navigate back to the detail page.
    assertNotEquals(
        bluetoothDeviceDetailPage.shadowRoot!.activeElement,
        getChangeKeyboardSettings());
    windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;
    await waitAfterNextRender(bluetoothDeviceDetailPage);

    assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    // Check that |#changeKeyboardSettings| has been focused.
    assertEquals(
        bluetoothDeviceDetailPage.shadowRoot!.activeElement,
        getChangeKeyboardSettings());

    device1.deviceProperties.deviceType = DeviceType.kKeyboardMouseCombo;
    bluetoothConfig.updatePairedDevice(device1);

    await flushTasks();
    assertTrue(!!getChangeMouseSettings());
    assertTrue(!!getChangeKeyboardSettings());

    // This is needed or other tests will fail.
    // TODO(gordonseto): Figure out how to remove this.
    getChangeKeyboardSettings()!.click();
    await flushTasks();
  });

  test('Device becomes unavailable while viewing pages.', async () => {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const windowPopstatePromise = eventToPromise('popstate', window);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '12345/6789&',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    const device2 = createDefaultBluetoothDevice(
        /*id=*/ '987654321',
        /*publicName=*/ 'MX 3',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device2',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothConfig.appendToPairedDeviceList([device1, device2]);
    await flushTasks();

    const params = new URLSearchParams();
    params.append('id', '12345/6789&');
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    await flushTasks();
    assertEquals(
        'device1',
        (bluetoothDeviceDetailPage.parentNode as OsSettingsSubpageElement)
            .pageTitle);
    assertTrue(!!bluetoothDeviceDetailPage.getDeviceIdForTest());

    // Device becomes unavailable in the devices list subpage. We should still
    // have a device id present since the device id would not be reset to an
    // empty string.
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES, params);
    await flushTasks();
    bluetoothConfig.removePairedDevice(device1);
    await flushTasks();
    assertTrue(!!bluetoothDeviceDetailPage.getDeviceIdForTest());

    // Add device back and check for when device becomes unavailable in
    // the device detail subpage.
    bluetoothConfig.appendToPairedDeviceList([device1]);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    bluetoothConfig.removePairedDevice(device1);

    // Device id is removed and navigation backward should occur.
    await windowPopstatePromise;
    assertEquals('', bluetoothDeviceDetailPage.getDeviceIdForTest());
  });

  test('Device UI states test', async () => {
    init();

    const getDeviceTypeIcon = () => {
      return bluetoothDeviceDetailPage.shadowRoot!.querySelector(
          'bluetooth-icon');
    };

    const getBluetoothDeviceNameLabel = () => {
      const label = bluetoothDeviceDetailPage.shadowRoot!.querySelector(
          '#bluetoothDeviceNameLabel');
      assertTrue(!!label);
      return label;
    };

    const getBluetoothDeviceBatteryInfo = () =>
        bluetoothDeviceDetailPage.shadowRoot!.querySelector('#batteryInfo');

    const getNonAudioOutputDeviceMessage = () =>
        bluetoothDeviceDetailPage.shadowRoot!.querySelector(
            '#nonAudioOutputDeviceMessage');

    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    assertTrue(!!getDeviceTypeIcon());
    assertTrue(!!getBluetoothStateText());
    assertTrue(!!getBluetoothDeviceNameLabel());
    assertNull(getBluetoothForgetBtn());
    assertNull(getBluetoothConnectDisconnectBtn());
    assertNull(getBluetoothDeviceBatteryInfo());
    assertNull(getNonAudioOutputDeviceMessage());

    const deviceNickname = 'device1';
    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '123456789',
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected, deviceNickname,
        /*opt_udioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kHeadset);

    device1.deviceProperties.batteryInfo = {
        ...getDefaultDeviceBatteryInfo(),
      defaultProperties: {batteryPercentage: 90},
    };
    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushTasks();

    navigateToDeviceDetailPage('123456789');
    await flushTasks();

    assertTrue(!!getBluetoothForgetBtn());

    // Simulate connected state and audio capable.
    assertTrue(!!getBluetoothConnectDisconnectBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        getBluetoothStateText().textContent!.trim());
    assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    assertEquals(
        deviceNickname, getBluetoothDeviceNameLabel().textContent!.trim());
    assertTrue(!!getBluetoothDeviceBatteryInfo());
    assertNull(getNonAudioOutputDeviceMessage());

    // Simulate disconnected state and not audio capable.
    device1.deviceProperties.connectionState =
        DeviceConnectionState.kNotConnected;
    device1.deviceProperties.audioCapability =
        AudioOutputCapability.kNotCapableOfAudioOutput;
    device1.deviceProperties.batteryInfo = {
        ...getDefaultDeviceBatteryInfo(),
    };
    bluetoothConfig.updatePairedDevice(device1);
    await flushTasks();

    assertNull(getBluetoothConnectDisconnectBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        getBluetoothStateText().textContent!.trim());
    assertFalse(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    assertNull(getBluetoothDeviceBatteryInfo());
    assertEquals(
        bluetoothDeviceDetailPage.i18n(
            'bluetoothDeviceDetailForgetA11yLabel', deviceNickname),
        getBluetoothForgetBtn()!.ariaLabel);
    assertTrue(!!getNonAudioOutputDeviceMessage());
    assertEquals(
        bluetoothDeviceDetailPage.i18n(
            'bluetoothDeviceDetailHIDMessageDisconnected'),
        getNonAudioOutputDeviceMessage()!.textContent!.trim());

    // Simulate connected state and not audio capable.
    device1.deviceProperties.connectionState = DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushTasks();

    assertTrue(!!getNonAudioOutputDeviceMessage());
    assertEquals(
        bluetoothDeviceDetailPage.i18n(
            'bluetoothDeviceDetailHIDMessageConnected'),
        getNonAudioOutputDeviceMessage()!.textContent!.trim());

    device1.deviceProperties.audioCapability =
        AudioOutputCapability.kCapableOfAudioOutput;
    bluetoothConfig.updatePairedDevice(device1);
    // Navigate away from details subpage with while connected and navigate
    // back.
    const windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;

    navigateToDeviceDetailPage('123456789');
    await flushTasks();

    assertTrue(!!getBluetoothConnectDisconnectBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        getBluetoothStateText().textContent!.trim());
  });

  test(
      'Change device dialog is shown after change name button click',
      async () => {
        init();
        bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

        const getChangeDeviceNameDialog = () =>
            bluetoothDeviceDetailPage.shadowRoot!.querySelector(
                '#changeDeviceNameDialog');

        const device1 = createDefaultBluetoothDevice(
            /*id=*/ '12//345&6789',
            /*publicName=*/ 'BeatsX',
            /*connectionState=*/
            DeviceConnectionState.kConnected,
            /*opt_nickname=*/ 'device1',
            /*opt_audioCapability=*/
            AudioOutputCapability.kCapableOfAudioOutput,
            /*opt_deviceType=*/ DeviceType.kMouse);

        bluetoothConfig.appendToPairedDeviceList([device1]);
        await flushTasks();

        navigateToDeviceDetailPage('12//345&6789');
        await flushTasks();

        assertNull(getChangeDeviceNameDialog());

        const changeNameBtn =
            bluetoothDeviceDetailPage.shadowRoot!
                .querySelector<HTMLButtonElement>('#changeNameBtn');
        assertTrue(!!changeNameBtn);
        changeNameBtn.click();

        await flushTasks();
        assertTrue(!!getChangeDeviceNameDialog());
      });

  test('Landing on page while device is still connecting', async () => {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const id = '12//345&6789';
    const device1 = createDefaultBluetoothDevice(
        /*id=*/ id,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnecting,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushTasks();

    navigateToDeviceDetailPage(id);
    await flushTasks();
    assertTrue(!!getBluetoothConnectDisconnectBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothConnecting'),
        getBluetoothStateText().textContent!.trim());
    assertNull(getConnectionFailedText());
    assertTrue(getBluetoothConnectDisconnectBtn()!.disabled);
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'),
        getBluetoothConnectDisconnectBtn()!.textContent!.trim());
  });

  test('Connect/Disconnect/forget states and error message', async () => {
    loadTimeData.overrideValues({'enableFastPairFlag': true});
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const windowPopstatePromise = eventToPromise('popstate', window);

    const getBluetoothDialogForgetButton = () => {
      const forgetDeviceDialog =
          bluetoothDeviceDetailPage.shadowRoot!.querySelector(
              '#forgetDeviceDialog');
      assertTrue(!!forgetDeviceDialog);
      const button =
          forgetDeviceDialog.shadowRoot!.querySelector<HTMLButtonElement>(
              '#forget');
      assertTrue(!!button);
      return button;
    };

    const assertUIState =
        (isShowingConnectionFailed: boolean,
         isConnectDisconnectBtnDisabled: boolean, bluetoothStateText: string,
         connectDisconnectBtnText: string) => {
          assertEquals(isShowingConnectionFailed, !!getConnectionFailedText());
          assertEquals(
              isConnectDisconnectBtnDisabled,
              getBluetoothConnectDisconnectBtn()!.disabled);
          assertEquals(
              bluetoothStateText, getBluetoothStateText().textContent!.trim());
          assertEquals(
              connectDisconnectBtnText,
              getBluetoothConnectDisconnectBtn()!.textContent!.trim());
        };

    const id = '12345/6789&';
    const device1 = createDefaultBluetoothDevice(
        id,
        /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kNotConnected,
        /*opt_nickname=*/ 'device1',
        /*opt_audioCapability=*/
        AudioOutputCapability.kCapableOfAudioOutput,
        /*opt_deviceType=*/ DeviceType.kMouse);

    device1.deviceProperties.batteryInfo = {
        ...getDefaultDeviceBatteryInfo(),
      defaultProperties: {batteryPercentage: 90},
    };

    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushTasks();

    navigateToDeviceDetailPage(id);

    await flushTasks();
    assertTrue(!!getBluetoothConnectDisconnectBtn());
    // Disconnected without error.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Try to connect.
    getBluetoothConnectDisconnectBtn()!.click();
    await flushTasks();
    // Connecting.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ true,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnecting'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));
    bluetoothConfig.completeConnect(/*success=*/ false);

    // Connection fails.
    await flushTasks();
    // Disconnected with error.
    assertUIState(
        /*isShowingConnectionFailed=*/ true,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Change device while connection failed text is shown.
    bluetoothConfig.appendToPairedDeviceList([Object.assign({}, device1)]);
    await flushTasks();

    // Disconnected with error.
    assertUIState(
        /*isShowingConnectionFailed=*/ true,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Try to connect with success.
    getBluetoothConnectDisconnectBtn()!.click();
    await flushTasks();
    bluetoothConfig.completeConnect(/*success=*/ true);
    // Connection success.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));

    // Disconnect device and check that connection error is not shown.
    getBluetoothConnectDisconnectBtn()!.click();

    // Disconnecting.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ true,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));
    await flushTasks();
    bluetoothConfig.completeDisconnect(/*success=*/ true);
    await flushTasks();
    // Disconnected without error.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Try to connect with error.
    getBluetoothConnectDisconnectBtn()!.click();
    await flushTasks();
    bluetoothConfig.completeConnect(/*success=*/ false);
    await flushTasks();
    // Disconnected with error.
    assertUIState(
        /*isShowingConnectionFailed=*/ true,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Device automatically reconnects without calling connect. This
    // can happen if connection failure was because device was turned off
    // and is turned on. We expect connection error text to not show when
    // disconnected.
    device1.deviceProperties.connectionState = DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushTasks();
    // Connection success.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));

    // Disconnect device and check that connection error is not shown.
    getBluetoothConnectDisconnectBtn()!.click();
    await flushTasks();
    bluetoothConfig.completeDisconnect(/*success=*/ true);
    await flushTasks();
    // Disconnected without error.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Retry connection with success.
    getBluetoothConnectDisconnectBtn()!.click();
    await flushTasks();
    // Connecting.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ true,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnecting'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));
    bluetoothConfig.completeConnect(/*success=*/ true);
    await flushTasks();
    // Connection success.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));

    const forgetDialogOpen =
        eventToPromise('cr-dialog-open', bluetoothDeviceDetailPage);

    // Forget device.
    getBluetoothForgetBtn()!.click();
    await flushTasks();
    await forgetDialogOpen;
    getBluetoothDialogForgetButton().click();

    await flushTasks();
    bluetoothConfig.completeForget(/*success=*/ true);
    await windowPopstatePromise;

    // Device and device Id should be null after navigating backward.
    assertNull(bluetoothDeviceDetailPage.getDeviceForTest());
    assertEquals('', bluetoothDeviceDetailPage.getDeviceIdForTest());
  });

  test('Route to device details page', () => {
    init();
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());
    navigateToDeviceDetailPage('id');
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');
  });
});
