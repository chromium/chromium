// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsBluetoothDevicesSubpageElement, SettingsPairedBluetoothListElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes, settingMojom, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {setHidPreservingControllerForTesting} from 'chrome://resources/ash/common/bluetooth/hid_preserving_bluetooth_state_controller.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {BluetoothSystemProperties, BluetoothSystemState, DeviceConnectionState, SystemPropertiesObserverInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://webui-test/chromeos/bluetooth/fake_bluetooth_config.js';
import {FakeHidPreservingBluetoothStateController} from 'chrome://webui-test/chromeos/bluetooth/fake_hid_preserving_bluetooth_state_controller.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('<os-settings-bluetooth-devices-subpage>', () => {
  let bluetoothConfig: FakeBluetoothConfig;
  let bluetoothDevicesSubpage: SettingsBluetoothDevicesSubpageElement;
  let propertiesObserver: SystemPropertiesObserverInterface;
  let browserProxy: TestOsBluetoothDevicesSubpageBrowserProxy;
  let hidPreservingController: FakeHidPreservingBluetoothStateController;

  suiteSetup(() => {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);
  });

  setup(() => {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
    loadTimeData.overrideValues({isCrossDeviceFeatureSuiteEnabled: true});
  });

  teardown(() => {
    bluetoothDevicesSubpage.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  async function init(
      isBluetoothDisconnectWarningEnabled: boolean = false,
      urlParams?: URLSearchParams): Promise<void> {
    if (isBluetoothDisconnectWarningEnabled) {
      loadTimeData.overrideValues({'bluetoothDisconnectWarningFlag': true});
      hidPreservingController = new FakeHidPreservingBluetoothStateController();
      hidPreservingController.setBluetoothConfigForTesting(bluetoothConfig);
      setHidPreservingControllerForTesting(hidPreservingController);
    } else {
      loadTimeData.overrideValues({'bluetoothDisconnectWarningFlag': false});
    }

    bluetoothDevicesSubpage =
        document.createElement('os-settings-bluetooth-devices-subpage');
    setFastPairPrefEnabled(true);
    document.body.appendChild(bluetoothDevicesSubpage);
    flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override properties
       */
      onPropertiesUpdated(properties: BluetoothSystemProperties) {
        bluetoothDevicesSubpage.systemProperties = properties;
      },
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES, urlParams);
    await flushTasks();
  }

  function setFastPairPrefEnabled(enabled: boolean): void {
    bluetoothDevicesSubpage.prefs = {
      ash: {fast_pair: {enabled: {value: enabled}}},
    };
  }

  test('Base Test', async () => {
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());
    await init();
    assertTrue(!!bluetoothDevicesSubpage);
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');
  });

  test('Only show saved devices link row when flag is true', async () => {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    loadTimeData.overrideValues({enableSavedDevicesFlag: true});
    await init();

    assertTrue(isVisible(bluetoothDevicesSubpage.shadowRoot!.querySelector(
        '#savedDevicesRowLink')));

    bluetoothDevicesSubpage.remove();
    // Set flag to False and check that the row is not visible.
    loadTimeData.overrideValues({enableSavedDevicesFlag: false});
    bluetoothDevicesSubpage =
        document.createElement('os-settings-bluetooth-devices-subpage');
    document.body.appendChild(bluetoothDevicesSubpage);
    flush();
    assertFalse(isVisible(bluetoothDevicesSubpage.shadowRoot!.querySelector(
        '#savedDevicesRowLink')));
  });

  test(
      'Hide saved devices link row when Cross Device suite disabled',
      async () => {
        bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
        loadTimeData.overrideValues({
          isCrossDeviceFeatureSuiteEnabled: false,
          enableSavedDevicesFlag: true,
        });
        await init();

        assertFalse(isVisible(bluetoothDevicesSubpage.shadowRoot!.querySelector(
            '#savedDevicesRowLink')));
      });

  test(
      'Selecting saved devices row routes to saved devices subpage',
      async () => {
        bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
        loadTimeData.overrideValues({enableSavedDevicesFlag: true});
        await init();

        const link =
            bluetoothDevicesSubpage.shadowRoot!
                .querySelector<HTMLButtonElement>('#savedDevicesRowLink');
        assertTrue(!!link);
        link.click();
        await flushTasks();
        assertEquals(
            routes.BLUETOOTH_SAVED_DEVICES, Router.getInstance().currentRoute);
      });

  test('Toggle button creation and a11y', async () => {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await init();
    const toggle =
        bluetoothDevicesSubpage.shadowRoot!.querySelector<CrToggleElement>(
            '#enableBluetoothToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    let a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    toggle.click();
    let a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        bluetoothDevicesSubpage.i18n('bluetoothDisabledA11YLabel')));

    a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    toggle.click();

    a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        bluetoothDevicesSubpage.i18n('bluetoothEnabledA11YLabel')));

    // Mock systemState becoming unavailable.
    a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);

    a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        bluetoothDevicesSubpage.i18n('bluetoothDisabledA11YLabel')));
  });

  test('Toggle button states', async () => {
    await init();

    const enableBluetoothToggle =
        bluetoothDevicesSubpage.shadowRoot!.querySelector<CrToggleElement>(
            '#enableBluetoothToggle');
    assertTrue(!!enableBluetoothToggle);

    const assertToggleEnabledState = (enabled: boolean) => {
      assertEquals(enabled, enableBluetoothToggle.checked);
      const element =
          bluetoothDevicesSubpage.shadowRoot!.querySelector<HTMLElement>(
              '.primary-toggle');
      assertTrue(!!element);
      assertEquals(
          bluetoothDevicesSubpage.i18n(enabled ? 'deviceOn' : 'deviceOff'),
          element.innerText);
    };
    assertToggleEnabledState(/*enabled=*/ false);

    // Simulate clicking toggle.
    enableBluetoothToggle.click();
    await flushTasks();

    // Toggle should be on since systemState is enabling.
    assertToggleEnabledState(/*enabled=*/ true);

    // Mock operation failing.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ false);
    await flushTasks();

    // Toggle should be off again.
    assertToggleEnabledState(/*enabled=*/ false);

    // Click again.
    enableBluetoothToggle.click();
    await flushTasks();

    // Toggle should be on since systemState is enabling.
    assertToggleEnabledState(/*enabled=*/ true);

    // Mock operation success.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushTasks();

    // Toggle should still be on.
    assertToggleEnabledState(/*enabled=*/ true);

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushTasks();
    assertToggleEnabledState(/*enabled=*/ false);
    assertTrue(enableBluetoothToggle.disabled);
  });

  test('Bluetooth toggle affects Fast Pair toggle', async () => {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await init();

    const enableBluetoothToggle =
        bluetoothDevicesSubpage.shadowRoot!.querySelector(
            '#enableBluetoothToggle');
    assertTrue(!!enableBluetoothToggle);

    const enableFastPairToggle =
        bluetoothDevicesSubpage.shadowRoot!.querySelector(
            '#enableFastPairToggle');
    assertTrue(!!enableFastPairToggle);
    const fastPairToggle =
        enableFastPairToggle.shadowRoot!
            .querySelector<SettingsToggleButtonElement>('#toggle');
    assertTrue(!!fastPairToggle);

    // Bluetooth is enabled, so Fast Pair should reset to pref (enabled).
    assertTrue(fastPairToggle.checked);
    assertFalse(fastPairToggle.disabled);

    // Toggle off Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushTasks();

    // Bluetooth is disabled, so Fast Pair should be off and disabled.
    assertFalse(fastPairToggle.checked);
    assertTrue(fastPairToggle.disabled);

    // Toggle on Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabling);
    await flushTasks();

    // Bluetooth is enabling, so Fast Pair should reset to pref (enabled).
    assertTrue(fastPairToggle.checked);
    assertFalse(fastPairToggle.disabled);

    // Toggle off Fast Pair pref.
    setFastPairPrefEnabled(false);
    await flushTasks();

    // Bluetooth is enabling, so Fast Pair should reset to pref (disabled).
    assertFalse(fastPairToggle.checked);
    assertFalse(fastPairToggle.disabled);

    // Toggle off Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabling);
    await flushTasks();

    // Bluetooth is disabling, so Fast Pair should be off and disabled.
    assertFalse(fastPairToggle.checked);
    assertTrue(fastPairToggle.disabled);

    // Toggle on Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await flushTasks();

    // Bluetooth is enabling, so Fast Pair should reset to pref (disabled).
    assertFalse(fastPairToggle.checked);
    assertFalse(fastPairToggle.disabled);
  });

  test('Device lists states', async () => {
    await init();

    function getNoDeviceText() {
      return bluetoothDevicesSubpage.shadowRoot!.querySelector('#noDevices');
    }

    function getDeviceList(connected: boolean) {
      return bluetoothDevicesSubpage.shadowRoot!
          .querySelector<SettingsPairedBluetoothListElement>(
              connected ? '#connectedDeviceList' : '#unconnectedDeviceList');
    }
    // No lists should be showing at first.
    assertNull(getDeviceList(/*connected=*/ true));
    assertNull(getDeviceList(/*connected=*/ false));
    assertTrue(!!getNoDeviceText());
    assertEquals(
        bluetoothDevicesSubpage.i18n('bluetoothDeviceListNoConnectedDevices'),
        getNoDeviceText()!.textContent?.trim());

    const connectedDevice = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected);
    const notConnectedDevice = createDefaultBluetoothDevice(
        /*id=*/ '987654321', /*publicName=*/ 'MX 3',
        /*connectionState=*/
        DeviceConnectionState.kNotConnected);
    const connectingDevice = createDefaultBluetoothDevice(
        /*id=*/ '11111111', /*publicName=*/ 'MX 3',
        /*connectionState=*/
        DeviceConnectionState.kConnecting);

    // Pair connected device.
    bluetoothConfig.appendToPairedDeviceList([connectedDevice]);
    await flushTasks();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(1, getDeviceList(/*connected=*/ true)!.devices.length);
    assertNull(getDeviceList(/*connected=*/ false));
    assertNull(getNoDeviceText());

    // Pair not connected device
    bluetoothConfig.appendToPairedDeviceList([notConnectedDevice]);
    await flushTasks();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(1, getDeviceList(/*connected=*/ true)!.devices.length);
    assertTrue(!!getDeviceList(/*connected=*/ false));
    assertEquals(1, getDeviceList(/*connected=*/ false)!.devices.length);
    assertNull(getNoDeviceText());

    // Pair connecting device
    bluetoothConfig.appendToPairedDeviceList([connectingDevice]);
    await flushTasks();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(1, getDeviceList(/*connected=*/ true)!.devices.length);
    assertTrue(!!getDeviceList(/*connected=*/ false));
    assertEquals(2, getDeviceList(/*connected=*/ false)!.devices.length);
    assertNull(getNoDeviceText());

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushTasks();

    assertNull(getDeviceList(/*connected=*/ true));
    assertNull(getDeviceList(/*connected=*/ false));
    assertTrue(!!getNoDeviceText());
  });

  test('Device list items are focused on backward navigation', async () => {
    await init();

    function getDeviceList(connected: boolean) {
      const element = bluetoothDevicesSubpage.shadowRoot!
                          .querySelector<SettingsPairedBluetoothListElement>(
                              connected ? '#connectedDeviceList' :
                                          '#unconnectedDeviceList');
      assertTrue(!!element);
      return element;
    }

    function getDeviceListItem(connected: boolean, index: number) {
      return getDeviceList(connected).shadowRoot!.querySelectorAll(
          'os-settings-paired-bluetooth-list-item')[index];
    }

    const connectedDeviceId = '1';
    const connectedDevice = createDefaultBluetoothDevice(
        /*id=*/ connectedDeviceId, /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected);
    const unconnectedDeviceId = '2';
    const unconnectedDevice = createDefaultBluetoothDevice(
        /*id=*/ unconnectedDeviceId, /*publicName=*/ 'MX 3',
        /*connectionState=*/
        DeviceConnectionState.kNotConnected);
    bluetoothConfig.appendToPairedDeviceList([connectedDevice]);
    bluetoothConfig.appendToPairedDeviceList([unconnectedDevice]);
    await flushTasks();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(1, getDeviceList(/*connected=*/ true).devices.length);
    assertTrue(!!getDeviceList(/*connected=*/ false));
    assertEquals(1, getDeviceList(/*connected=*/ false).devices.length);

    // Simulate navigating to the detail page of |connectedDevice|.
    let params = new URLSearchParams();
    params.append('id', connectedDeviceId);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    await flushTasks();

    // Navigate backwards.
    assertNotEquals(
        getDeviceListItem(/*connected=*/ true, /*index=*/ 0),
        getDeviceList(/*connected=*/ true).shadowRoot!.activeElement);
    let windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;

    // The first connected device list item should be focused.
    assertEquals(
        getDeviceListItem(/*connected=*/ true, /*index=*/ 0),
        getDeviceList(/*connected=*/ true).shadowRoot!.activeElement);

    // Simulate navigating to the detail page of |unconnectedDevice|.
    params = new URLSearchParams();
    params.append('id', unconnectedDeviceId);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
    await flushTasks();

    // Navigate backwards.
    assertNotEquals(
        getDeviceListItem(/*connected=*/ false, /*index=*/ 0),
        getDeviceList(/*connected=*/ false).shadowRoot!.activeElement);
    windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;

    // The first unconnected device list item should be focused.
    assertEquals(
        getDeviceListItem(/*connected=*/ false, /*index=*/ 0),
        getDeviceList(/*connected=*/ false).shadowRoot!.activeElement);
  });

  test('Deep link to enable/disable Bluetooth toggle button', async () => {
    flush();
    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kBluetoothOnOff.toString());
    await init(false, params);

    const deepLinkElement = bluetoothDevicesSubpage.shadowRoot!.querySelector(
        '#enableBluetoothToggle');
    await waitAfterNextRender(bluetoothDevicesSubpage);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'On startup enable/disable Bluetooth toggle should be focused for settingId=100.');
  });

  // TODO(b/215724676): Re-enable this test once the suite is migrated to
  // interactive UI tests. Focus is currently flaky in browser tests.
  test.skip('Deep link to enable/disable Fast pair toggle button', async () => {
    flush();
    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kFastPairOnOff.toString());
    await init(false, params);

    const fastPairToggle = bluetoothDevicesSubpage.shadowRoot!.querySelector(
        '#enableFastPairToggle');
    assertTrue(!!fastPairToggle);
    const innerToggle = fastPairToggle.shadowRoot!.querySelector('#toggle');
    assertTrue(!!innerToggle);
    const deepLinkElement = innerToggle.shadowRoot!.querySelector('#control');
    await waitAfterNextRender(bluetoothDevicesSubpage);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Enable Fast Pair toggle should be focused for settingId=105.');
  });

  test('Show saved devices link row when flag is true', async () => {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    loadTimeData.overrideValues({isGuest: false, enableSavedDevicesFlag: true});
    await init();

    assertTrue(isVisible(bluetoothDevicesSubpage.shadowRoot!.querySelector(
        '#savedDevicesRowLink')));
  });

  test('Do not show saved devices link row when flag is false', async () => {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    loadTimeData.overrideValues(
        {isGuest: false, enableSavedDevicesFlag: false});
    await init();

    assertFalse(isVisible(bluetoothDevicesSubpage.shadowRoot!.querySelector(
        '#savedDevicesRowLink')));
  });

  test('Do not show saved devices link row in guest mode', async () => {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    loadTimeData.overrideValues({isGuest: true, enableSavedDevicesFlag: true});
    await init();

    assertFalse(isVisible(bluetoothDevicesSubpage.shadowRoot!.querySelector(
        '#savedDevicesRowLink')));
  });

  test('Single separator line when Fast Pair UI disabled', async () => {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    loadTimeData.overrideValues({'enableFastPairFlag': false});
    await init();

    const sepLines = bluetoothDevicesSubpage.shadowRoot!.querySelectorAll(
        '.device-lists-separator');
    assertEquals(1, sepLines.length);
  });

  test('Greater than 1 separator line when Fast Pair UI enabled', async () => {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    loadTimeData.overrideValues({'enableFastPairFlag': true});
    await init();

    const sepLines = bluetoothDevicesSubpage.shadowRoot!.querySelectorAll(
        '.device-lists-separator');
    assertGT(sepLines.length, 1);
  });

  test('Toggle Bluetooth with bluetoothDisconnectWarningFlag on', async () => {
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushTasks();
    await init(/* isBluetoothDisconnectWarningEnabled= */ true);

    const enableBluetoothToggle =
        bluetoothDevicesSubpage.shadowRoot!.querySelector<CrToggleElement>(
            '#enableBluetoothToggle');
    assertTrue(!!enableBluetoothToggle);
    assertFalse(enableBluetoothToggle.checked);

    const enableBluetooth = async () => {
      assertTrue(
          bluetoothDevicesSubpage.systemProperties.systemState ===
          BluetoothSystemState.kDisabled);

      // Simulate clicking toggle.
      enableBluetoothToggle.click();
      await flushTasks();

      // Toggle should be on since systemState is enabling.
      assertTrue(
          bluetoothDevicesSubpage.systemProperties.systemState ===
          BluetoothSystemState.kEnabling);

      // Mock operation success.
      bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
      await flushTasks();
      assertTrue(
          bluetoothDevicesSubpage.systemProperties.systemState ===
          BluetoothSystemState.kEnabled);
    };

    await enableBluetooth();
    assertEquals(hidPreservingController.getDialogShownCount(), 0);

    // Disable bluetooth and simulate showing dialog, with user electing
    // to continue disabling Bluetooth.
    hidPreservingController.setShouldShowWarningDialog(true);
    enableBluetoothToggle.click();
    await flushTasks();

    assertTrue(enableBluetoothToggle.checked);
    assertEquals(hidPreservingController.getDialogShownCount(), 1);
    assertTrue(
        bluetoothDevicesSubpage.systemProperties.systemState ===
        BluetoothSystemState.kEnabled);
    hidPreservingController.completeShowDialog(true);
    await flushTasks();

    assertFalse(enableBluetoothToggle.checked);
    assertTrue(
        bluetoothDevicesSubpage.systemProperties.systemState ===
        BluetoothSystemState.kDisabling);
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushTasks();
    await enableBluetooth();
    assertEquals(hidPreservingController.getDialogShownCount(), 1);
    assertTrue(enableBluetoothToggle.checked);

    // Disable Bluetooth and simulate showing dialog with user selecting
    // to keep current bluetooth state.
    enableBluetoothToggle.click();
    await flushTasks();

    assertTrue(enableBluetoothToggle.checked);
    assertEquals(hidPreservingController.getDialogShownCount(), 2);
    assertTrue(
        bluetoothDevicesSubpage.systemProperties.systemState ===
        BluetoothSystemState.kEnabled);
    hidPreservingController.completeShowDialog(false);

    await flushTasks();
    assertTrue(enableBluetoothToggle.checked);
    assertTrue(
        bluetoothDevicesSubpage.systemProperties.systemState ===
        BluetoothSystemState.kEnabled);
  });
});
