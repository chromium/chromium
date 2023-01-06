// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';

import {OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {mojoString16ToString} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {BluetoothSystemProperties, BluetoothSystemState, DeviceConnectionState, SystemPropertiesObserverInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createDefaultBluetoothDevice, FakeBluetoothConfig} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('OsBluetoothSummaryTest', function() {
  /** @type {!FakeBluetoothConfig} */
  let bluetoothConfig;

  /** @type {!SettingsBluetoothSummaryElement|undefined} */
  let bluetoothSummary;

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
    bluetoothSummary = document.createElement('os-settings-bluetooth-summary');
    document.body.appendChild(bluetoothSummary);
    flush();

    propertiesObserver = {
      /**
       * SystemPropertiesObserverInterface override
       * @param {!BluetoothSystemProperties}
       *     properties
       */
      onPropertiesUpdated(properties) {
        bluetoothSummary.systemProperties = properties;
      },
    };
    bluetoothConfig.observeSystemProperties(propertiesObserver);
  }

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Route to Bluetooth devices subpage', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushAsync();
    const iconButton =
        bluetoothSummary.shadowRoot.querySelector('#arrowIconButton');
    assertTrue(!!iconButton);

    iconButton.click();
    assertEquals(
        Router.getInstance().getCurrentRoute(), routes.BLUETOOTH_DEVICES);
    assertNotEquals(
        iconButton, bluetoothSummary.shadowRoot.activeElement,
        'subpage icon should not be focused');

    // Navigate back to the top-level page.
    const windowPopstatePromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await windowPopstatePromise;
    await waitBeforeNextRender(bluetoothSummary);

    // Check that |iconButton| has been focused.
    assertEquals(
        iconButton, bluetoothSummary.shadowRoot.activeElement,
        'subpage icon should be focused');
  });

  test('Toggle button creation and a11y', async function() {
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await flushAsync();
    init();
    let a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);

    const toggle =
        bluetoothSummary.shadowRoot.querySelector('#enableBluetoothToggle');
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

  test('Toggle button states', async function() {
    init();
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());

    const enableBluetoothToggle =
        bluetoothSummary.shadowRoot.querySelector('#enableBluetoothToggle');
    assertTrue(!!enableBluetoothToggle);
    assertFalse(enableBluetoothToggle.checked);

    // Simulate clicking toggle.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertTrue(enableBluetoothToggle.checked);
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');

    // Mock operation failing.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ false);
    await flushAsync();

    // Toggle should be off again.
    assertFalse(enableBluetoothToggle.checked);
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to remain the same');

    // Click again.
    enableBluetoothToggle.click();
    await flushAsync();

    // Toggle should be on since systemState is enabling.
    assertTrue(enableBluetoothToggle.checked);
    assertEquals(
        2, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');

    // Mock operation success.
    bluetoothConfig.completeSetBluetoothEnabledState(/*success=*/ true);
    await flushAsync();

    // Toggle should still be on.
    assertTrue(enableBluetoothToggle.checked);
    assertEquals(
        2, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to remain the same');

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertTrue(enableBluetoothToggle.disabled);
    assertFalse(enableBluetoothToggle.checked);
    assertEquals(
        2, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to remain the same');
  });

  test('UI states test', async function() {
    init();

    // Simulate device state is disabled.
    const bluetoothSecondaryLabel =
        bluetoothSummary.shadowRoot.querySelector('#bluetoothSecondaryLabel');
    const getBluetoothArrowIconBtn = () =>
        bluetoothSummary.shadowRoot.querySelector('#arrowIconButton');
    const getBluetoothStatusIcon = () =>
        bluetoothSummary.shadowRoot.querySelector('#statusIcon');
    const getSecondaryLabel = () => bluetoothSecondaryLabel.textContent.trim();
    const getPairNewDeviceBtn = () =>
        bluetoothSummary.shadowRoot.querySelector('#pairNewDeviceBtn');

    assertFalse(!!getBluetoothArrowIconBtn());
    assertTrue(!!getBluetoothStatusIcon());
    assertFalse(!!getPairNewDeviceBtn());
    assertTrue(!!bluetoothSecondaryLabel);

    assertEquals(
        bluetoothSummary.i18n('bluetoothSummaryPageOff'), getSecondaryLabel());
    assertEquals(
        'os-settings:bluetooth-disabled', getBluetoothStatusIcon().icon);

    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushAsync();

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
    await flushAsync();

    assertEquals(
        'os-settings:bluetooth-connected', getBluetoothStatusIcon().icon);
    assertEquals(
        bluetoothSummary.i18n(
            'bluetoothSummaryPageTwoOrMoreDevicesDescription', device1.nickname,
            mockPairedBluetoothDeviceProperties.length - 1),
        getSecondaryLabel());

    // Simulate 2 connected devices.
    bluetoothConfig.removePairedDevice(device3);
    await flushAsync();

    assertEquals(
        bluetoothSummary.i18n(
            'bluetoothSummaryPageTwoDevicesDescription', device1.nickname,
            mojoString16ToString(device2.deviceProperties.publicName)),
        getSecondaryLabel());

    // Simulate a single connected device.
    bluetoothConfig.removePairedDevice(device2);
    await flushAsync();

    assertEquals(device1.nickname, getSecondaryLabel());

    /// Simulate no connected device.
    bluetoothConfig.removePairedDevice(device1);
    await flushAsync();

    assertEquals(
        bluetoothSummary.i18n('bluetoothSummaryPageOn'), getSecondaryLabel());
    assertEquals('cr:bluetooth', getBluetoothStatusIcon().icon);
    assertTrue(!!getPairNewDeviceBtn());

    // Mock systemState becoming unavailable.
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushAsync();
    assertFalse(!!getBluetoothArrowIconBtn());
    assertFalse(!!getPairNewDeviceBtn());
    assertEquals(
        bluetoothSummary.i18n('bluetoothSummaryPageOff'), getSecondaryLabel());
    assertEquals(
        'os-settings:bluetooth-disabled', getBluetoothStatusIcon().icon);
  });

  test('start-pairing is fired on pairNewDeviceBtn click', async function() {
    init();
    bluetoothConfig.setBluetoothEnabledState(/*enabled=*/ true);
    await flushAsync();

    const toggleBluetoothPairingUiPromise =
        eventToPromise('start-pairing', bluetoothSummary);
    const getPairNewDeviceBtn = () =>
        bluetoothSummary.shadowRoot.querySelector('#pairNewDeviceBtn');

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
    const bluetoothSummaryPrimary =
        bluetoothSummary.shadowRoot.querySelector('#bluetoothSummary');
    const bluetoothSummarySecondary =
        bluetoothSummary.shadowRoot.querySelector('#bluetoothSummarySeconday');
    const bluetoothSummarySecondaryText =
        bluetoothSummary.shadowRoot.querySelector(
            '#bluetoothSummarySecondayText');

    assertFalse(!!bluetoothSummaryPrimary);
    assertTrue(!!bluetoothSummarySecondary);

    assertEquals(
        bluetoothSummary.i18n(
            'bluetoothPrimaryUserControlled', primaryUserEmail),
        bluetoothSummarySecondaryText.textContent.trim());
  });

  test('Route to summary page', function() {
    init();
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());
    Router.getInstance().navigateTo(routes.BLUETOOTH);
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');
  });

});
