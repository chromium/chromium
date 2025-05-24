// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import type {OsSettingsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import type {CrIconButtonElement, SettingsBluetoothPageElement} from 'chrome://os-settings/os_settings.js';
import {OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {setBluetoothConfigForTesting} from 'chrome://resources/ash/common/bluetooth/cros_bluetooth_config.js';
import {BluetoothSystemState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeBluetoothConfig} from 'chrome://webui-test/chromeos/bluetooth/fake_bluetooth_config.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('<os-settings-bluetooth-page>', () => {
  let bluetoothConfig: FakeBluetoothConfig;
  let bluetoothPage: SettingsBluetoothPageElement;
  let browserProxy: TestOsBluetoothDevicesSubpageBrowserProxy;

  async function init(): Promise<void> {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);

    bluetoothConfig = new FakeBluetoothConfig();
    setBluetoothConfigForTesting(bluetoothConfig);
    bluetoothPage = document.createElement('os-settings-bluetooth-page');
    document.body.appendChild(bluetoothPage);
    await flushTasks();
  }

  teardown(() => {
    bluetoothPage.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('Show bluetooth pairing UI', async () => {
    await init();
    const getBluetoothPairingUi = () => bluetoothPage.shadowRoot!.querySelector(
        'os-settings-bluetooth-pairing-dialog');
    const bluetoothSummary = bluetoothPage.shadowRoot!.querySelector(
        'os-settings-bluetooth-summary');

    const getPairNewDevice = () =>
        bluetoothPage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#pairNewDevice');

    assertTrue(!!bluetoothSummary);
    assertNull(getBluetoothPairingUi());
    assertEquals(0, browserProxy.getShowBluetoothRevampHatsSurveyCount());

    bluetoothSummary.dispatchEvent(new CustomEvent('start-pairing'));

    await flushTasks();
    assertTrue(!!getBluetoothPairingUi());
    assertEquals(
        1, browserProxy.getShowBluetoothRevampHatsSurveyCount(),
        'Count failed to increase');

    getBluetoothPairingUi()!.dispatchEvent(new CustomEvent('close'));

    await flushTasks();
    assertNull(getBluetoothPairingUi());

    Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);

    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await flushTasks();
    assertTrue(!!getPairNewDevice());

    // Simulate Bluetooth disabled
    bluetoothConfig.setSystemState(BluetoothSystemState.kDisabled);
    await flushTasks();
    assertNull(getPairNewDevice());

    // Simulate Bluetooth unavailable
    bluetoothConfig.setSystemState(BluetoothSystemState.kUnavailable);
    await flushTasks();
    assertNull(getPairNewDevice());

    // Simulate Bluetooth being enabled.
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabling);
    await flushTasks();
    assertNull(getPairNewDevice());

    // Simulate Bluetooth enabled.
    bluetoothConfig.setSystemState(BluetoothSystemState.kEnabled);
    await flushTasks();

    const pairNewDeviceButton = getPairNewDevice();
    assertTrue(!!pairNewDeviceButton);
    pairNewDeviceButton.click();

    await flushTasks();
    assertTrue(!!getBluetoothPairingUi());
  });

  suite('back button on the landing page', () => {
    let backButton: CrIconButtonElement;
    let bluetoothSubpage: OsSettingsSubpageElement;

    setup(async () => {
      await init();

      Router.getInstance().navigateTo(routes.BLUETOOTH_DEVICES);
      await flushTasks();

      const subpageElement =
          bluetoothPage.shadowRoot!.querySelector<OsSettingsSubpageElement>(
              'os-settings-subpage');
      assertTrue(!!subpageElement);
      bluetoothSubpage = subpageElement;

      const iconButtonElement =
          bluetoothSubpage.shadowRoot!.querySelector<CrIconButtonElement>(
              '#backButton');
      assertTrue(!!iconButtonElement);
      backButton = iconButtonElement;
    });

    // The bluetooth top-level page is inactive in order to reduce the number
    // of clicks to change bluetooth settings. The bluetooth subpage replaces
    // the top-level page for now, so the back button should not be visible.
    // See b/309808834 for more details on reactivating the original top-level
    // page.
    test('is hidden', () => {
      assertFalse(isVisible(backButton));
    });
  });
});
