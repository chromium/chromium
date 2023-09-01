// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';

import {OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {AudioOutputCapability, BluetoothSystemProperties, DeviceConnectionState, DeviceType, SystemPropertiesObserverInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('OsBluetoothDeviceDetailPageTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothDeviceDetailSubpageElement|undefined} */
  let bluetoothDeviceDetailPage;

  /**
   * @type {!SystemPropertiesObserverInterface}
   */
  let propertiesObserver;

  /** @type {?OsBluetoothDevicesSubpageBrowserProxy} */
  let browserProxy = null;

  setup(function() {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  function init() {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);

    bluetoothDeviceDetailPage =
        document.createElement('os-settings-bluetooth-device-detail-subpage');
    document.body.appendChild(bluetoothDeviceDetailPage);
    flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override
       * @param {!BluetoothSystemProperties}
       *     properties
       */
      onPropertiesUpdated(properties) {
        bluetoothDeviceDetailPage.systemProperties = properties;
      },
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
    flush();
  }

  function flushAsync() {
    flush();
    return new Promise((resolve) => setTimeout(resolve));
  }

  teardown(function() {
    bluetoothDeviceDetailPage.remove();
    bluetoothDeviceDetailPage = null;
    Router.getInstance().resetRouteForTesting();
  });


  test(
      'Error text is not shown after navigating away from page',
      async function() {
        init();
        bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
        const windowPopstatePromise = eventToPromise('popstate', window);

        const getBluetoothConnectDisconnectBtn = () =>
            bluetoothDeviceDetailPage.shadowRoot.querySelector(
                '#connectDisconnectBtn');
        const getConnectionFailedText = () =>
            bluetoothDeviceDetailPage.shadowRoot.querySelector(
                '#connectionFailed');

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
          defaultProperties: {batteryPercentage: 90},
        };

        bluetoothConfig.appendToPairedDeviceList([device1]);
        await flushAsync();

        let params = new URLSearchParams();
        params.append('id', id);
        Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);

        await flushAsync();

        // Try to connect.
        getBluetoothConnectDisconnectBtn().click();
        await flushAsync();
        bluetoothConfig.completeConnect(/*success=*/ false);
        await flushAsync();
        assertTrue(!!getConnectionFailedText());

        params = new URLSearchParams();
        params.append('id', id);
        Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
        await flushAsync();
        assertFalse(!!getConnectionFailedText());
      });

  test('Managed by enterprise icon', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const getManagedIcon = () => {
      return bluetoothDeviceDetailPage.shadowRoot.querySelector('#managedIcon');
    };

    const navigateToDeviceDetailPage = () => {
      const params = new URLSearchParams();
      params.append('id', '12345/6789&');
      Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    };

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
    await flushAsync();

    navigateToDeviceDetailPage();

    await flushAsync();
    assertTrue(!!getManagedIcon());

    device.deviceProperties.isBlockedByPolicy = false;
    bluetoothConfig.updatePairedDevice(device);
    await flushAsync();
    assertFalse(!!getManagedIcon());
  });

  test(
      'True Wireless Images not shown when Fast pair disabled',
      async function() {
        loadTimeData.overrideValues({'enableFastPairFlag': false});
        init();
        bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

        const getTrueWirelessImages = () =>
            bluetoothDeviceDetailPage.shadowRoot.querySelector(
                '#trueWirelessImages');

        const navigateToDeviceDetailPage = () => {
          const params = new URLSearchParams();
          params.append('id', '12345/6789&');
          Router.getInstance().navigateTo(
              routes.BLUETOOTH_DEVICE_DETAIL, params);
        };

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
            caseImageUrl: fakeUrl,
            rightBudImageUrl: fakeUrl,
          },
        };
        device.deviceProperties.batteryInfo = {
          leftBudInfo: {batteryPercentage: 90},
        };

        bluetoothConfig.appendToPairedDeviceList([device]);
        await flushAsync();

        navigateToDeviceDetailPage();

        // Since Fast Pair flag is false, we don't show the component.
        await flushAsync();
        assertFalse(!!getTrueWirelessImages());
      });

  test('True Wireless Images shown when expected', async function() {
    loadTimeData.overrideValues({'enableFastPairFlag': true});
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const getTrueWirelessImages = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#trueWirelessImages');

    const navigateToDeviceDetailPage = () => {
      const params = new URLSearchParams();
      params.append('id', '12345/6789&');
      Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    };

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
      trueWirelessImages: {leftBudImageUrl: fakeUrl, caseImageUrl: fakeUrl},
    };
    device.deviceProperties.batteryInfo = {
      leftBudInfo: {batteryPercentage: 90},
    };

    bluetoothConfig.appendToPairedDeviceList([device]);
    await flushAsync();

    navigateToDeviceDetailPage();

    // Don't display component unless either the default image is
    // present OR all of the true wireless images are present.
    await flushAsync();
    assertFalse(!!getTrueWirelessImages());

    // Try again with all 3 True Wireless images.
    device.deviceProperties.imageInfo.trueWirelessImages.rightBudImageUrl =
        fakeUrl;
    bluetoothConfig.updatePairedDevice(device);
    await flushAsync();
    assertTrue(!!getTrueWirelessImages());

    // Try again with just default image.
    device.deviceProperties.imageInfo = {
      defaultImageUrl: fakeUrl,
    };
    bluetoothConfig.updatePairedDevice(device);
    await flushAsync();
    assertTrue(!!getTrueWirelessImages());

    // If battery info is not available, only show True Wireless
    // component if not connected.
    device.deviceProperties.batteryInfo = {};
    device.deviceProperties.connectionState =
        DeviceConnectionState.kNotConnected;
    bluetoothConfig.updatePairedDevice(device);
    await flushAsync();
    assertTrue(!!getTrueWirelessImages());

    device.deviceProperties.connectionState = DeviceConnectionState.kConnecting;
    bluetoothConfig.updatePairedDevice(device);
    await flushAsync();
    assertTrue(!!getTrueWirelessImages());

    device.deviceProperties.connectionState = DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device);
    await flushAsync();
    assertFalse(!!getTrueWirelessImages());

    // Having either default battery info or True Wireless battery info
    // should show True Wireless component if device is connected.
    device.deviceProperties.batteryInfo = {
      defaultProperties: {batteryPercentage: 90},
    };
    bluetoothConfig.updatePairedDevice(device);
    await flushAsync();
    assertTrue(!!getTrueWirelessImages());

    device.deviceProperties.batteryInfo = {
      rightBudInfo: {batteryPercentage: 90},
    };
    bluetoothConfig.updatePairedDevice(device);
    await flushAsync();
    assertTrue(!!getTrueWirelessImages());
  });

  test(
      'Show change settings row, and navigate to subpages w/ no per-device settings',
      async function() {
        loadTimeData.overrideValues({
          enableInputDeviceSettingsSplit: false,
        });

        init();
        bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

        const getChangeMouseSettings = () =>
            bluetoothDeviceDetailPage.shadowRoot.querySelector(
                '#changeMouseSettings');
        const getChangeKeyboardSettings = () =>
            bluetoothDeviceDetailPage.shadowRoot.querySelector(
                '#changeKeyboardSettings');

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
        await flushAsync();

        const params = new URLSearchParams();
        params.append('id', '12//345&6789');
        Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);

        device1.deviceProperties.connectionState =
            DeviceConnectionState.kNotConnected;
        bluetoothConfig.updatePairedDevice(device1);
        await flushAsync();
        assertFalse(!!getChangeMouseSettings());

        device1.deviceProperties.connectionState =
            DeviceConnectionState.kConnected;
        bluetoothConfig.updatePairedDevice(device1);
        await flushAsync();
        assertTrue(!!getChangeMouseSettings());

        getChangeMouseSettings().click();
        await flushAsync();

        assertEquals(Router.getInstance().currentRoute, routes.POINTERS);

        // Navigate back to the detail page.
        assertNotEquals(
            getChangeMouseSettings(),
            bluetoothDeviceDetailPage.shadowRoot.activeElement);
        let windowPopstatePromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await windowPopstatePromise;
        await waitAfterNextRender(bluetoothDeviceDetailPage);

        assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
        // Check that |#changeMouseSettings| has been focused.
        assertEquals(
            getChangeMouseSettings(),
            bluetoothDeviceDetailPage.shadowRoot.activeElement);

        device1.deviceProperties.deviceType = DeviceType.kKeyboard;
        bluetoothConfig.updatePairedDevice(device1);

        await flushAsync();
        assertFalse(!!getChangeMouseSettings());
        assertTrue(!!getChangeKeyboardSettings());

        device1.deviceProperties.connectionState =
            DeviceConnectionState.kNotConnected;
        bluetoothConfig.updatePairedDevice(device1);
        await flushAsync();
        assertFalse(!!getChangeKeyboardSettings());

        device1.deviceProperties.connectionState =
            DeviceConnectionState.kConnected;
        bluetoothConfig.updatePairedDevice(device1);
        await flushAsync();
        assertTrue(!!getChangeKeyboardSettings());

        getChangeKeyboardSettings().click();
        await flushAsync();

        assertEquals(Router.getInstance().currentRoute, routes.KEYBOARD);

        // Navigate back to the detail page.
        assertNotEquals(
            getChangeKeyboardSettings(),
            bluetoothDeviceDetailPage.shadowRoot.activeElement);
        windowPopstatePromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await windowPopstatePromise;
        await waitAfterNextRender(bluetoothDeviceDetailPage);

        // This is needed or other tests will fail.
        // TODO(gordonseto): Figure out how to remove this.
        getChangeKeyboardSettings().click();
        await flushAsync();
      });

  test('Show change settings row, and navigate to subpages', async function() {
    loadTimeData.overrideValues({
      enableInputDeviceSettingsSplit: true,
    });

    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const getChangeMouseSettings = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#changeMouseSettings');
    const getChangeKeyboardSettings = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#changeKeyboardSettings');

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
    await flushAsync();

    assertFalse(!!getChangeMouseSettings());
    assertFalse(!!getChangeKeyboardSettings());

    const params = new URLSearchParams();
    params.append('id', '12//345&6789');
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
    assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    assertTrue(!!getChangeMouseSettings());
    assertFalse(!!getChangeKeyboardSettings());
    assertEquals(
        bluetoothDeviceDetailPage.i18n(
            'bluetoothDeviceDetailChangeDeviceSettingsMouse'),
        getChangeMouseSettings().label);

    device1.deviceProperties.connectionState =
        DeviceConnectionState.kNotConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushAsync();
    assertFalse(!!getChangeMouseSettings());

    device1.deviceProperties.connectionState = DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushAsync();
    assertTrue(!!getChangeMouseSettings());

    getChangeMouseSettings().click();
    await flushAsync();

    assertEquals(Router.getInstance().currentRoute, routes.PER_DEVICE_MOUSE);

    // Navigate back to the detail page.
    assertNotEquals(
        getChangeMouseSettings(),
        bluetoothDeviceDetailPage.shadowRoot.activeElement);
    let windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;
    await waitAfterNextRender(bluetoothDeviceDetailPage);

    assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    // Check that |#changeMouseSettings| has been focused.
    assertEquals(
        getChangeMouseSettings(),
        bluetoothDeviceDetailPage.shadowRoot.activeElement);

    device1.deviceProperties.deviceType = DeviceType.kKeyboard;
    bluetoothConfig.updatePairedDevice(device1);

    await flushAsync();
    assertFalse(!!getChangeMouseSettings());
    assertTrue(!!getChangeKeyboardSettings());

    device1.deviceProperties.connectionState =
        DeviceConnectionState.kNotConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushAsync();
    assertFalse(!!getChangeKeyboardSettings());

    device1.deviceProperties.connectionState = DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushAsync();
    assertTrue(!!getChangeKeyboardSettings());

    getChangeKeyboardSettings().click();
    await flushAsync();

    assertEquals(Router.getInstance().currentRoute, routes.PER_DEVICE_KEYBOARD);

    // Navigate back to the detail page.
    assertNotEquals(
        getChangeKeyboardSettings(),
        bluetoothDeviceDetailPage.shadowRoot.activeElement);
    windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;
    await waitAfterNextRender(bluetoothDeviceDetailPage);

    assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    // Check that |#changeKeyboardSettings| has been focused.
    assertEquals(
        getChangeKeyboardSettings(),
        bluetoothDeviceDetailPage.shadowRoot.activeElement);

    device1.deviceProperties.deviceType = DeviceType.kKeyboardMouseCombo;
    bluetoothConfig.updatePairedDevice(device1);

    await flushAsync();
    assertTrue(!!getChangeMouseSettings());
    assertTrue(!!getChangeKeyboardSettings());

    // This is needed or other tests will fail.
    // TODO(gordonseto): Figure out how to remove this.
    getChangeKeyboardSettings().click();
    await flushAsync();
  });

  test('Device becomes unavailable while viewing pages.', async function() {
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
    await flushAsync();

    const params = new URLSearchParams();
    params.append('id', '12345/6789&');
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    await flushAsync();
    assertEquals('device1', bluetoothDeviceDetailPage.parentNode.pageTitle);
    assertTrue(!!bluetoothDeviceDetailPage.getDeviceIdForTest());

    // Device becomes unavailable in the devices list subpage. We should still
    // have a device id present since the device id would not be reset to an
    // empty string.
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES, params);
    await flushAsync();
    bluetoothConfig.removePairedDevice(device1);
    await flushAsync();
    assertTrue(!!bluetoothDeviceDetailPage.getDeviceIdForTest());

    // Add device back and check for when device becomes unavailable in
    // the device detail subpage.
    bluetoothConfig.appendToPairedDeviceList([device1]);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    bluetoothConfig.removePairedDevice(device1);

    // Device id is removed and navigation backward should occur.
    await windowPopstatePromise;
    assertFalse(!!bluetoothDeviceDetailPage.getDeviceIdForTest());
  });

  test('Device UI states test', async function() {
    init();
    const getBluetoothStatusIcon = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector('#statusIcon');
    const getBluetoothStateText = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#bluetoothStateText');
    const getBluetoothForgetBtn = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector('#forgetBtn');
    const getBluetoothStateBtn = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#connectDisconnectBtn');
    const getBluetoothDeviceNameLabel = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#bluetoothDeviceNameLabel');
    const getBluetoothDeviceBatteryInfo = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector('#batteryInfo');
    const getNonAudioOutputDeviceMessage = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#nonAudioOutputDeviceMessage');

    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    assertTrue(!!getBluetoothStatusIcon());
    assertTrue(!!getBluetoothStateText());
    assertTrue(!!getBluetoothDeviceNameLabel());
    assertFalse(!!getBluetoothForgetBtn());
    assertFalse(!!getBluetoothStateBtn());
    assertFalse(!!getBluetoothDeviceBatteryInfo());
    assertFalse(!!getNonAudioOutputDeviceMessage());

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
      defaultProperties: {batteryPercentage: 90},
    };
    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushAsync();

    let params = new URLSearchParams();
    params.append('id', '123456789');
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    await flushAsync();

    assertTrue(!!getBluetoothForgetBtn());

    // Simulate connected state and audio capable.
    assertTrue(!!getBluetoothStateBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        getBluetoothStateText().textContent.trim());
    assertTrue(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    assertEquals(
        deviceNickname, getBluetoothDeviceNameLabel().textContent.trim());
    assertEquals(
        'os-settings:bluetooth-connected', getBluetoothStatusIcon().icon);
    assertTrue(!!getBluetoothDeviceBatteryInfo());
    assertFalse(!!getNonAudioOutputDeviceMessage());

    // Simulate disconnected state and not audio capable.
    device1.deviceProperties.connectionState =
        DeviceConnectionState.kNotConnected;
    device1.deviceProperties.audioCapability =
        AudioOutputCapability.kNotCapableOfAudioOutput;
    device1.deviceProperties.batteryInfo = {defaultProperties: null};
    bluetoothConfig.updatePairedDevice(device1);
    await flushAsync();

    assertFalse(!!getBluetoothStateBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        getBluetoothStateText().textContent.trim());
    assertFalse(bluetoothDeviceDetailPage.getIsDeviceConnectedForTest());
    assertEquals(
        'os-settings:bluetooth-disabled', getBluetoothStatusIcon().icon);
    assertFalse(!!getBluetoothDeviceBatteryInfo());
    assertEquals(
        getBluetoothForgetBtn().ariaLabel,
        bluetoothDeviceDetailPage.i18n(
            'bluetoothDeviceDetailForgetA11yLabel', deviceNickname));
    assertTrue(!!getNonAudioOutputDeviceMessage());
    assertEquals(
        bluetoothDeviceDetailPage.i18n(
            'bluetoothDeviceDetailHIDMessageDisconnected'),
        getNonAudioOutputDeviceMessage().textContent.trim());

    // Simulate connected state and not audio capable.
    device1.deviceProperties.connectionState = DeviceConnectionState.kConnected;
    bluetoothConfig.updatePairedDevice(device1);
    await flushAsync();

    assertTrue(!!getNonAudioOutputDeviceMessage());
    assertEquals(
        bluetoothDeviceDetailPage.i18n(
            'bluetoothDeviceDetailHIDMessageConnected'),
        getNonAudioOutputDeviceMessage().textContent.trim());

    device1.deviceProperties.audioCapability =
        AudioOutputCapability.kCapableOfAudioOutput;
    bluetoothConfig.updatePairedDevice(device1);
    // Navigate away from details subpage with while connected and navigate
    // back.
    const windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;

    params = new URLSearchParams();
    params.append('id', '123456789');
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    await flushAsync();

    assertTrue(!!getBluetoothStateBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        getBluetoothStateText().textContent.trim());
  });

  test(
      'Change device dialog is shown after change name button click',
      async function() {
        init();
        bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

        const getChangeDeviceNameDialog = () =>
            bluetoothDeviceDetailPage.shadowRoot.querySelector(
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
        await flushAsync();

        const params = new URLSearchParams();
        params.append('id', '12//345&6789');
        Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);

        await flushAsync();

        assertFalse(!!getChangeDeviceNameDialog());

        const changeNameBtn =
            bluetoothDeviceDetailPage.shadowRoot.querySelector(
                '#changeNameBtn');
        assertTrue(!!changeNameBtn);
        changeNameBtn.click();

        await flushAsync();
        assertTrue(!!getChangeDeviceNameDialog());
      });

  test('Landing on page while device is still connecting', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const getBluetoothConnectDisconnectBtn = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#connectDisconnectBtn');
    const getBluetoothStateText = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#bluetoothStateText');
    const getConnectionFailedText = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector('#connectionFailed');

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
    await flushAsync();

    const params = new URLSearchParams();
    params.append('id', id);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
    await flushAsync();
    assertTrue(!!getBluetoothConnectDisconnectBtn());
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothConnecting'),
        getBluetoothStateText().textContent.trim());
    assertFalse(!!getConnectionFailedText());
    assertTrue(getBluetoothConnectDisconnectBtn().disabled);
    assertEquals(
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'),
        getBluetoothConnectDisconnectBtn().textContent.trim());
  });

  test('Connect/Disconnect/forget states and error message', async function() {
    loadTimeData.overrideValues({'enableFastPairFlag': true});
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);

    const windowPopstatePromise = eventToPromise('popstate', window);

    const getBluetoothForgetBtn = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector('#forgetBtn');

    const getBluetoothDialogForgetButton = () =>
        bluetoothDeviceDetailPage.shadowRoot
            .querySelector('#forgetDeviceDialog')
            .shadowRoot.querySelector('#forget');

    const getBluetoothConnectDisconnectBtn = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#connectDisconnectBtn');

    const getBluetoothStateText = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector(
            '#bluetoothStateText');
    const getConnectionFailedText = () =>
        bluetoothDeviceDetailPage.shadowRoot.querySelector('#connectionFailed');

    const assertUIState =
        (isShowingConnectionFailed, isConnectDisconnectBtnDisabled,
         bluetoothStateText, connectDisconnectBtnText) => {
          assertEquals(!!getConnectionFailedText(), isShowingConnectionFailed);
          assertEquals(
              getBluetoothConnectDisconnectBtn().disabled,
              isConnectDisconnectBtnDisabled);
          assertEquals(
              getBluetoothStateText().textContent.trim(), bluetoothStateText);
          assertEquals(
              getBluetoothConnectDisconnectBtn().textContent.trim(),
              connectDisconnectBtnText);
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
      defaultProperties: {batteryPercentage: 90},
    };

    bluetoothConfig.appendToPairedDeviceList([device1]);
    await flushAsync();

    const params = new URLSearchParams();
    params.append('id', id);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);

    await flushAsync();
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
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
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
    await flushAsync();
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
    await flushAsync();

    // Disconnected with error.
    assertUIState(
        /*isShowingConnectionFailed=*/ true,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Try to connect with success.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
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
    getBluetoothConnectDisconnectBtn().click();

    // Disconnecting.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ true,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));
    await flushAsync();
    bluetoothConfig.completeDisconnect(/*success=*/ true);
    await flushAsync();
    // Disconnected without error.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));

    // Try to connect with error.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
    bluetoothConfig.completeConnect(/*success=*/ false);
    await flushAsync();
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
    await flushAsync();
    // Connection success.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));

    // Disconnect device and check that connection error is not shown.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
    bluetoothConfig.completeDisconnect(/*success=*/ true);
    await flushAsync();
    // Disconnected without error.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailDisconnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));


    // Retry connection with success.
    getBluetoothConnectDisconnectBtn().click();
    await flushAsync();
    // Connecting.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ true,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnecting'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothConnect'));
    bluetoothConfig.completeConnect(/*success=*/ true);
    await flushAsync();
    // Connection success.
    assertUIState(
        /*isShowingConnectionFailed=*/ false,
        /*isConnectDisconnectBtnDisabled=*/ false,
        /*bluetoothStateText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDeviceDetailConnected'),
        /*connectDisconnectBtnText=*/
        bluetoothDeviceDetailPage.i18n('bluetoothDisconnect'));

    const forgetDialogOpen = eventToPromise(
        'cr-dialog-open', /** @type {!Element} */ (bluetoothDeviceDetailPage));
    // Forget device.
    getBluetoothForgetBtn().click();
    await flushAsync();
    await forgetDialogOpen;
    getBluetoothDialogForgetButton().click();

    await flushAsync();
    bluetoothConfig.completeForget(/*success=*/ true);
    await windowPopstatePromise;

    // Device and device Id should be null after navigating backward.
    assertFalse(!!bluetoothDeviceDetailPage.getDeviceForTest());
    assertFalse(!!bluetoothDeviceDetailPage.getDeviceIdForTest());
  });

  test('Route to device details page', function() {
    init();
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());
    const params = new URLSearchParams();
    params.append('id', 'id');
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');
  });
});
