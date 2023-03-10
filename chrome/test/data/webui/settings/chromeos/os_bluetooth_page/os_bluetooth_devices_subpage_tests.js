// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';

import {OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {BluetoothSystemProperties, BluetoothSystemState, DeviceConnectionState, SystemPropertiesObserverInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('OsBluetoothDevicesSubpageTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothDevicesSubpageElement|undefined} */
  let bluetoothDevicesSubpage;

  /**
   * @type {!SystemPropertiesObserverInterface}
   */
  let propertiesObserver;

  /** @type {?OsBluetoothDevicesSubpageBrowserProxy} */
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);

    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  teardown(function() {
    bluetoothDevicesSubpage.remove();
    bluetoothDevicesSubpage = null;
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * @param {URLSearchParams=} opt_urlParams
   * @return {!Promise}
   */
  function init(opt_urlParams) {
    bluetoothDevicesSubpage =
        document.createElement('os-settings-bluetooth-devices-subpage');
    setFastPairPrefEnabled(true);
    document.body.appendChild(bluetoothDevicesSubpage);
    flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override
       * @param {!BluetoothSystemProperties}
       *     properties
       */
      onPropertiesUpdated(properties) {
        bluetoothDevicesSubpage.systemProperties = properties;
      },
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES, opt_urlParams);
    return flushAsync();
  }

  function setFastPairPrefEnabled(enabled) {
    bluetoothDevicesSubpage.prefs = {
      'ash': {'fast_pair': {'enabled': {value: enabled}}},
    };
  }

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Base Test', async function() {
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());
    await init();
    assertTrue(!!bluetoothDevicesSubpage);
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');
  });

  test('Only show saved devices link row when flag is true', async function() {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await init();

    bluetoothDevicesSubpage.remove();
    // Set flag to True and check that the row is visible.
    loadTimeData.overrideValues({'enableSavedDevicesFlag': true});
    bluetoothDevicesSubpage =
        document.createElement('os-settings-bluetooth-devices-subpage');
    document.body.appendChild(bluetoothDevicesSubpage);
    flush();
    assertTrue(isVisible(bluetoothDevicesSubpage.shadowRoot.querySelector(
        '#savedDevicesRowLink')));

    bluetoothDevicesSubpage.remove();
    // Set flag to False and check that the row is not visible.
    loadTimeData.overrideValues({'enableSavedDevicesFlag': false});
    bluetoothDevicesSubpage =
        document.createElement('os-settings-bluetooth-devices-subpage');
    document.body.appendChild(bluetoothDevicesSubpage);
    flush();
    assertFalse(isVisible(bluetoothDevicesSubpage.shadowRoot.querySelector(
        '#savedDevicesRowLink')));
  });

  test(
      'Selecting saved devices row routes to saved devices subpage',
      async function() {
        bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
        await init();

        bluetoothDevicesSubpage.remove();
        // Set flag to True and check that the row is visible.
        loadTimeData.overrideValues({'enableSavedDevicesFlag': true});
        bluetoothDevicesSubpage =
            document.createElement('os-settings-bluetooth-devices-subpage');
        document.body.appendChild(bluetoothDevicesSubpage);
        flush();

        bluetoothDevicesSubpage.shadowRoot.querySelector('#savedDevicesRowLink')
            .click();
        await flushAsync();
        assertEquals(
            Router.getInstance().currentRoute, routes.BLUETOOTH_SAVED_DEVICES);
      });

  test('Toggle button creation and a11y', async function() {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await init();
    const toggle = bluetoothDevicesSubpage.shadowRoot.querySelector(
        '#enableBluetoothToggle');
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
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertToggleEnabledState(/*enabled=*/ false);
    assertTrue(enableBluetoothToggle.disabled);
  });

  test('Bluetooth toggle affects Fast Pair toggle', async function() {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await init();

    const enableBluetoothToggle =
        bluetoothDevicesSubpage.$.enableBluetoothToggle;
    assertTrue(!!enableBluetoothToggle);

    const enableFastPairToggle =
        bluetoothDevicesSubpage.shadowRoot.querySelector(
            '#enableFastPairToggle');
    const fastPairToggle = enableFastPairToggle.$.toggle;
    assertTrue(!!fastPairToggle);

    // Bluetooth is enabled, so Fast Pair should reset to pref (enabled).
    assertTrue(fastPairToggle.checked);
    assertFalse(fastPairToggle.disabled);

    // Toggle off Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushAsync();

    // Bluetooth is disabled, so Fast Pair should be off and disabled.
    assertFalse(fastPairToggle.checked);
    assertTrue(fastPairToggle.disabled);

    // Toggle on Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabling);
    await flushAsync();

    // Bluetooth is enabling, so Fast Pair should reset to pref (enabled).
    assertTrue(fastPairToggle.checked);
    assertFalse(fastPairToggle.disabled);

    // Toggle off Fast Pair pref.
    setFastPairPrefEnabled(false);
    await flushAsync();

    // Bluetooth is enabling, so Fast Pair should reset to pref (disabled).
    assertFalse(fastPairToggle.checked);
    assertFalse(fastPairToggle.disabled);

    // Toggle off Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabling);
    await flushAsync();

    // Bluetooth is disabling, so Fast Pair should be off and disabled.
    assertFalse(fastPairToggle.checked);
    assertTrue(fastPairToggle.disabled);

    // Toggle on Bluetooth.
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await flushAsync();

    // Bluetooth is enabling, so Fast Pair should reset to pref (disabled).
    assertFalse(fastPairToggle.checked);
    assertFalse(fastPairToggle.disabled);
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
    await flushAsync();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(getDeviceList(/*connected=*/ true).devices.length, 1);
    assertFalse(!!getDeviceList(/*connected=*/ false));
    assertFalse(!!getNoDeviceText());

    // Pair not connected device
    bluetoothConfig.appendToPairedDeviceList([notConnectedDevice]);
    await flushAsync();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(getDeviceList(/*connected=*/ true).devices.length, 1);
    assertTrue(!!getDeviceList(/*connected=*/ false));
    assertEquals(getDeviceList(/*connected=*/ false).devices.length, 1);
    assertFalse(!!getNoDeviceText());

    // Pair connecting device
    bluetoothConfig.appendToPairedDeviceList([connectingDevice]);
    await flushAsync();

    assertTrue(!!getDeviceList(/*connected=*/ true));
    assertEquals(getDeviceList(/*connected=*/ true).devices.length, 1);
    assertTrue(!!getDeviceList(/*connected=*/ false));
    assertEquals(getDeviceList(/*connected=*/ false).devices.length, 2);
    assertFalse(!!getNoDeviceText());

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushAsync();

    assertFalse(!!getDeviceList(/*connected=*/ true));
    assertFalse(!!getDeviceList(/*connected=*/ false));
    assertTrue(!!getNoDeviceText());
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
            DeviceConnectionState.kConnected);
        const unconnectedDeviceId = '2';
        const unconnectedDevice = createDefaultBluetoothDevice(
            /*id=*/ unconnectedDeviceId, /*publicName=*/ 'MX 3',
            /*connectionState=*/
            DeviceConnectionState.kNotConnected);
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
        Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
        await flushAsync();

        // Navigate backwards.
        assertNotEquals(
            getDeviceListItem(/*connected=*/ true, /*index=*/ 0),
            getDeviceList(/*connected=*/ true).shadowRoot.activeElement);
        let windowPopstatePromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await windowPopstatePromise;

        // The first connected device list item should be focused.
        assertEquals(
            getDeviceListItem(/*connected=*/ true, /*index=*/ 0),
            getDeviceList(/*connected=*/ true).shadowRoot.activeElement);

        // Simulate navigating to the detail page of |unconnectedDevice|.
        params = new URLSearchParams();
        params.append('id', unconnectedDeviceId);
        Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICE_DETAIL, params);
        await flushAsync();

        // Navigate backwards.
        assertNotEquals(
            getDeviceListItem(/*connected=*/ false, /*index=*/ 0),
            getDeviceList(/*connected=*/ false).shadowRoot.activeElement);
        windowPopstatePromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await windowPopstatePromise;

        // The first unconnected device list item should be focused.
        assertEquals(
            getDeviceListItem(/*connected=*/ false, /*index=*/ 0),
            getDeviceList(/*connected=*/ false).shadowRoot.activeElement);
      });

  test('Deep link to enable/disable Bluetooth toggle button', async () => {
    flush();
    const params = new URLSearchParams();
    params.append('settingId', '100');
    init(params);

    const deepLinkElement = bluetoothDevicesSubpage.shadowRoot.querySelector(
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
    params.append('settingId', '105');
    init(params);

    const fastPairToggle = bluetoothDevicesSubpage.shadowRoot.querySelector(
        '#enableFastPairToggle');
    const innerToggle = fastPairToggle.shadowRoot.querySelector('#toggle');
    const deepLinkElement = innerToggle.shadowRoot.querySelector('#control');
    await waitAfterNextRender(bluetoothDevicesSubpage);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Enable Fast Pair toggle should be focused for settingId=105.');
  });

  test('Show saved devices link row when flag is true', async function() {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    loadTimeData.overrideValues({isGuest: false, enableSavedDevicesFlag: true});
    await init();

    assertTrue(isVisible(bluetoothDevicesSubpage.shadowRoot.querySelector(
        '#savedDevicesRowLink')));
  });

  test(
      'Do not show saved devices link row when flag is false',
      async function() {
        bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
        loadTimeData.overrideValues(
            {isGuest: false, enableSavedDevicesFlag: false});
        await init();

        assertFalse(isVisible(bluetoothDevicesSubpage.shadowRoot.querySelector(
            '#savedDevicesRowLink')));
      });

  test('Do not show saved devices link row in guest mode', async function() {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    loadTimeData.overrideValues({isGuest: true, enableSavedDevicesFlag: true});
    await init();

    assertFalse(isVisible(bluetoothDevicesSubpage.shadowRoot.querySelector(
        '#savedDevicesRowLink')));
  });

  test('Single separator line when Fast Pair UI disabled', async function() {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    loadTimeData.overrideValues({'enableFastPairFlag': false});
    await init();

    const sepLines = bluetoothDevicesSubpage.shadowRoot.querySelectorAll(
        '.device-lists-separator');
    assertEquals(sepLines.length, 1);
  });

  test(
      'Greater than 1 separator line when Fast Pair UI enabled',
      async function() {
        bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
        loadTimeData.overrideValues({'enableFastPairFlag': true});
        await init();

        const sepLines = bluetoothDevicesSubpage.shadowRoot.querySelectorAll(
            '.device-lists-separator');
        assertGT(sepLines.length, 1);
      });
});
