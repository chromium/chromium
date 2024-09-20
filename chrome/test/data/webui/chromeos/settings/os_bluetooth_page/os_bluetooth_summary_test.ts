// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {CrToggleElement, IronIconElement, OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes, SettingsBluetoothSummaryElement} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {setHidPreservingControllerForTesting} from 'chrome://resources/ash/common/bluetooth/hid_preserving_bluetooth_state_controller.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {BluetoothSystemProperties, BluetoothSystemState, DeviceConnectionState, SystemPropertiesObserverInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://webui-test/chromeos/bluetooth/fake_bluetooth_config.js';
import {FakeHidPreservingBluetoothStateController} from 'chrome://webui-test/chromeos/bluetooth/fake_hid_preserving_bluetooth_state_controller.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('<os-settings-bluetooth-summary>', () => {
  let bluetoothConfig: FakeBluetoothConfig;
  let bluetoothSummary: SettingsBluetoothSummaryElement;
  let propertiesObserver: SystemPropertiesObserverInterface;
  let browserProxy: TestOsBluetoothDevicesSubpageBrowserProxy;
  let hidPreservingController: FakeHidPreservingBluetoothStateController;

  setup(() => {
    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
  });

  teardown(() => {
    bluetoothSummary.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  async function init(isBluetoothDisconnectWarningEnabled: boolean = false) {
    if (isBluetoothDisconnectWarningEnabled) {
      loadTimeData.overrideValues({'bluetoothDisconnectWarningFlag': true});
      hidPreservingController = new FakeHidPreservingBluetoothStateController();
      hidPreservingController.setBluetoothConfigForTesting(bluetoothConfig);
      setHidPreservingControllerForTesting(hidPreservingController);
    } else {
      loadTimeData.overrideValues({'bluetoothDisconnectWarningFlag': false});
    }

    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);
    bluetoothSummary = document.createElement('os-settings-bluetooth-summary');
    document.body.appendChild(bluetoothSummary);
    flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override properties
       */
      onPropertiesUpdated(properties: BluetoothSystemProperties) {
        bluetoothSummary.systemProperties = properties;
      },
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
  }

  test('Toggle Bluetooth with bluetoothDisconnectWarningFlag on', async () => {
    await flushTasks();
    await init(/* isBluetoothDisconnectWarningEnabled= */ true);
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushTasks();

    const enableBluetoothToggle =
        bluetoothSummary.shadowRoot!.querySelector<CrToggleElement>(
            '#enableBluetoothToggle');
    assertTrue(!!enableBluetoothToggle);

    const enableBluetooth = async () => {
      assertTrue(
          bluetoothSummary.systemProperties.systemState ===
          BluetoothSystemState.kDisabled);

      // Simulate clicking toggle.
      enableBluetoothToggle.click();
      await flushTasks();

      // Toggle should be on since systemState is enabling.
      assertTrue(
          bluetoothSummary.systemProperties.systemState ===
          BluetoothSystemState.kEnabling);

      // Mock operation success.
      bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
      await flushTasks();
      assertTrue(
          bluetoothSummary.systemProperties.systemState ===
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
        bluetoothSummary.systemProperties.systemState ===
        BluetoothSystemState.kEnabled);
    hidPreservingController.completeShowDialog(true);
    await flushTasks();

    assertFalse(enableBluetoothToggle.checked);
    assertTrue(
        bluetoothSummary.systemProperties.systemState ===
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
        bluetoothSummary.systemProperties.systemState ===
        BluetoothSystemState.kEnabled);
    hidPreservingController.completeShowDialog(false);

    await flushTasks();
    assertTrue(enableBluetoothToggle.checked);
    assertTrue(
        bluetoothSummary.systemProperties.systemState ===
        BluetoothSystemState.kEnabled);
  });

  test('Button is focused after returning from devices subpage', async () => {
    await init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushTasks();
    const iconButton =
        bluetoothSummary.shadowRoot!.querySelector<HTMLButtonElement>(
            '#arrowIconButton');
    assertTrue(!!iconButton);

    iconButton.click();
    assertEquals(routes.BLUETOOTH_DEVICES, Router.getInstance().currentRoute);
    assertNotEquals(
        iconButton, bluetoothSummary.shadowRoot!.activeElement,
        'subpage icon should not be focused');

    // Navigate back to the top-level page.
    const windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;
    await waitAfterNextRender(bluetoothSummary);

    // Check that |iconButton| has been focused.
    assertEquals(
        iconButton, bluetoothSummary.shadowRoot!.activeElement,
        'subpage icon should be focused');
  });

  test('Toggle button creation and a11y', async () => {
    await init();
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await flushTasks();
    let a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);

    const toggle = bluetoothSummary.shadowRoot!.querySelector<CrToggleElement>(
        '#enableBluetoothToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    toggle.click();
    let a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        bluetoothSummary.i18n('bluetoothDisabledA11YLabel')));

    a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    toggle.click();

    a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        bluetoothSummary.i18n('bluetoothEnabledA11YLabel')));
  });

  test('Toggle button states', async () => {
    await init();
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushTasks();
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());

    const getPairNewDeviceBtn = () =>
        bluetoothSummary.shadowRoot!.querySelector('#pairNewDeviceBtn');

    const enableBluetoothToggle =
        bluetoothSummary.shadowRoot!.querySelector<CrToggleElement>(
            '#enableBluetoothToggle');
    assertTrue(!!enableBluetoothToggle);
    assertFalse(enableBluetoothToggle.checked);

    assertNull(getPairNewDeviceBtn());

    // Simulate clicking toggle.
    enableBluetoothToggle.click();
    await flushTasks();

    // Toggle should be on since systemState is enabling.
    assertTrue(enableBluetoothToggle.checked);
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');

    // Mock operation failing.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ false);
    await flushTasks();

    // Toggle should be off again.
    assertFalse(enableBluetoothToggle.checked);
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to remain the same');
    assertNull(getPairNewDeviceBtn());

    // Click again.
    enableBluetoothToggle.click();
    await flushTasks();

    // Toggle should be on since systemState is enabling.
    assertTrue(enableBluetoothToggle.checked);
    assertEquals(
        2, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');
    assertNull(getPairNewDeviceBtn());

    // Mock operation success.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushTasks();

    // Toggle should still be on.
    assertTrue(enableBluetoothToggle.checked);
    assertEquals(
        2, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to remain the same');
    assertTrue(!!getPairNewDeviceBtn());

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushTasks();
    assertTrue(enableBluetoothToggle.disabled);
    assertFalse(enableBluetoothToggle.checked);
    assertEquals(
        2, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to remain the same');
  });

  test('UI states test', async () => {
    await init();

    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushTasks();

    // Simulate device state is disabled.
    const bluetoothSecondaryLabel =
        bluetoothSummary.shadowRoot!.querySelector('#bluetoothSecondaryLabel');
    assertTrue(!!bluetoothSecondaryLabel);
    const getBluetoothArrowIconBtn = () =>
        bluetoothSummary.shadowRoot!.querySelector('#arrowIconButton');
    const getBluetoothStatusIcon = () => {
      const statusIcon =
          bluetoothSummary.shadowRoot!.querySelector<IronIconElement>(
              '#statusIcon');
      assertTrue(!!statusIcon);
      return statusIcon;
    };
    const getSecondaryLabel = () => bluetoothSecondaryLabel.textContent?.trim();
    const getPairNewDeviceBtn = () =>
        bluetoothSummary.shadowRoot!.querySelector('#pairNewDeviceBtn');

    assertNull(getBluetoothArrowIconBtn());
    assertTrue(!!getBluetoothStatusIcon());
    assertNull(getPairNewDeviceBtn());

    assertEquals(
        bluetoothSummary.i18n('bluetoothSummaryPageOff'), getSecondaryLabel());
    assertEquals(
        'os-settings:bluetooth-disabled', getBluetoothStatusIcon().icon);

    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushTasks();

    assertTrue(!!getBluetoothArrowIconBtn());
    assertNull(getPairNewDeviceBtn());
    // Bluetooth Icon should be default because no devices are connected.
    assertEquals('cr:bluetooth', getBluetoothStatusIcon().icon);

    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushTasks();

    assertTrue(!!getBluetoothArrowIconBtn());
    assertTrue(!!getPairNewDeviceBtn());
    // Bluetooth Icon should be default because no devices are connected.
    assertEquals('cr:bluetooth', getBluetoothStatusIcon().icon);

    const device1 = createDefaultBluetoothDevice(
        /*id=*/ '123456789', /*publicName=*/ 'BeatsX',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device1');
    const device2 = createDefaultBluetoothDevice(
        /*id=*/ '987654321', /*publicName=*/ 'MX 3',
        /*connectionState=*/
        DeviceConnectionState.kConnected);
    const device3 = createDefaultBluetoothDevice(
        /*id=*/ '456789', /*publicName=*/ 'Radio head',
        /*connectionState=*/
        DeviceConnectionState.kConnected,
        /*opt_nickname=*/ 'device3');

    const mockPairedBluetoothDeviceProperties = [
      device1,
      device2,
      device3,
    ];

    // Simulate 3 connected devices.
    bluetoothConfig.appendToPairedDeviceList(
        mockPairedBluetoothDeviceProperties);
    await flushTasks();

    assertEquals(
        'os-settings:bluetooth-connected', getBluetoothStatusIcon().icon);
    assertEquals(
        bluetoothSummary.i18n(
            'bluetoothSummaryPageTwoOrMoreDevicesDescription',
            device1.nickname!, mockPairedBluetoothDeviceProperties.length - 1),
        getSecondaryLabel());

    // Simulate 2 connected devices.
    bluetoothConfig.removePairedDevice(device3);
    await flushTasks();

    assertEquals(
        bluetoothSummary.i18n(
            'bluetoothSummaryPageTwoDevicesDescription', device1.nickname!,
            mojoString16ToString(device2.deviceProperties.publicName)),
        getSecondaryLabel());

    // Simulate a single connected device.
    bluetoothConfig.removePairedDevice(device2);
    await flushTasks();

    assertEquals(device1.nickname, getSecondaryLabel());

    /// Simulate no connected device.
    bluetoothConfig.removePairedDevice(device1);
    await flushTasks();

    assertEquals(
        bluetoothSummary.i18n('bluetoothSummaryPageOn'), getSecondaryLabel());
    assertEquals('cr:bluetooth', getBluetoothStatusIcon().icon);
    assertTrue(!!getPairNewDeviceBtn());

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushTasks();
    assertNull(getBluetoothArrowIconBtn());
    assertNull(getPairNewDeviceBtn());
    assertEquals(
        bluetoothSummary.i18n('bluetoothSummaryPageOff'), getSecondaryLabel());
    assertEquals(
        'os-settings:bluetooth-disabled', getBluetoothStatusIcon().icon);
  });

  test('start-pairing is fired on pairNewDeviceBtn click', async () => {
    await init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushTasks();

    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushTasks();

    const toggleBluetoothPairingUiPromise =
        eventToPromise('start-pairing', bluetoothSummary);
    const getPairNewDeviceBtn = () => {
      const button =
          bluetoothSummary.shadowRoot!.querySelector<HTMLButtonElement>(
              '#pairNewDeviceBtn');
      assertTrue(!!button);
      return button;
    };
    getPairNewDeviceBtn().click();

    await toggleBluetoothPairingUiPromise;
  });

  test('Secondary user', async () => {
    const primaryUserEmail = 'test@gmail.com';
    loadTimeData.overrideValues({
      isSecondaryUser: true,
      primaryUserEmail,
    });
    await init();

    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushTasks();
    const bluetoothSummaryPrimary =
        bluetoothSummary.shadowRoot!.querySelector('#bluetoothSummary');
    const bluetoothSummarySecondary =
        bluetoothSummary.shadowRoot!.querySelector('#bluetoothSummarySeconday');
    const bluetoothSummarySecondaryText =
        bluetoothSummary.shadowRoot!.querySelector(
            '#bluetoothSummarySecondayText');

    assertNull(bluetoothSummaryPrimary);
    assertTrue(!!bluetoothSummarySecondary);
    assertTrue(!!bluetoothSummarySecondaryText);

    assertEquals(
        bluetoothSummary.i18n(
            'bluetoothPrimaryUserControlled', primaryUserEmail),
        bluetoothSummarySecondaryText.textContent?.trim());
  });

  test('Route to summary page', async () => {
    await init();
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());
    Router.getInstance().navigateTo(routes.BLUETOOTH);
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');
  });
});
