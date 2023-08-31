// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsBluetoothSavedDevicesSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {FastPairSavedDevicesOptInStatus, OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('<os-settings-bluetooth-saved-devices-subpage>', () => {
  let savedDevicesSubpage: SettingsBluetoothSavedDevicesSubpageElement;
  let browserProxy: TestOsBluetoothDevicesSubpageBrowserProxy;

  suiteSetup(() => {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);
  });

  setup(() => {
    Router.getInstance().navigateTo(routes.BLUETOOTH_SAVED_DEVICES);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    savedDevicesSubpage.remove();
    browserProxy.reset();
  });

  async function init(): Promise<void> {
    savedDevicesSubpage =
        document.createElement('os-settings-bluetooth-saved-devices-subpage');
    document.body.appendChild(savedDevicesSubpage);
    await flushTasks();
  }

  function getLabel(): string {
    const label =
        savedDevicesSubpage.shadowRoot!.querySelector<HTMLElement>('#label');
    if (!label) {
      return '';
    }
    return label.innerText;
  }

  test('Base Test', async () => {
    await init();
    assertTrue(!!savedDevicesSubpage);
  });

  test('Show list when >0 saved devices', async () => {
    browserProxy.setSavedDevices(
        [{name: 'dev1', imageUrl: '', accountKey: '0'}]);
    await init();
    assertEquals(loadTimeData.getString('sublabelWithEmail'), getLabel());
  });

  test('Show label with no saved devices', async () => {
    await init();
    assertEquals(loadTimeData.getString('noDevicesWithEmail'), getLabel());
  });

  test('Show error label', async () => {
    browserProxy.setOptInStatus(
        FastPairSavedDevicesOptInStatus
            .STATUS_ERROR_RETRIEVING_FROM_FOOTPRINTS_SERVER);
    await init();
    assertEquals(
        loadTimeData.getString('savedDevicesErrorWithEmail'), getLabel());
  });

  test('Dynamic dialog change upon last device removal', async () => {
    browserProxy.setSavedDevices(
        [{name: 'dev1', imageUrl: '', accountKey: '0'}]);
    await init();
    assertEquals(loadTimeData.getString('sublabelWithEmail'), getLabel());

    const list =
        savedDevicesSubpage.shadowRoot!.querySelector('#savedDevicesList');
    assertTrue(!!list);
    const listItems = list.shadowRoot!.querySelectorAll(
        'os-settings-saved-devices-list-item');

    assertGT(listItems.length, 0);

    const listItem = listItems[0];
    assertTrue(!!listItem);

    const dotsMenu =
        listItem.shadowRoot!.querySelector<HTMLButtonElement>('#dotsMenu');
    assertTrue(!!dotsMenu);
    dotsMenu.click();
    await flushTasks();

    const removeButton =
        listItem.shadowRoot!.querySelector<HTMLButtonElement>('#removeButton');
    assertTrue(!!removeButton);
    removeButton.click();
    await flushTasks();

    const removeDialog =
        listItem.shadowRoot!.querySelector('#removeDeviceDialog');
    assertTrue(!!removeDialog);
    const removeBtn =
        removeDialog.shadowRoot!.querySelector<HTMLButtonElement>('#remove');
    assertTrue(!!removeBtn);
    removeBtn.click();

    await flushTasks();
    assertEquals(loadTimeData.getString('noDevicesWithEmail'), getLabel());
  });
});
