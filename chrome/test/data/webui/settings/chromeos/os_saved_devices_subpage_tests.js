// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';
import 'chrome://os-settings/strings.m.js';

import {FastPairSavedDevicesOptInStatus, OsBluetoothDevicesSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {DeviceConnectionState} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createDefaultBluetoothDevice} from 'chrome://webui-test/cr_components/chromeos/bluetooth/fake_bluetooth_config.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestOsBluetoothDevicesSubpageBrowserProxy} from './test_os_bluetooth_subpage_browser_proxy.js';

suite('OsSavedDevicesSubpageTest', function() {
  /** @type {?SettingsBluetoothSavedDevicesSubpageElement|undefined} */
  let savedDevicesSubpage = null;

  /** @type {?OsBluetoothDevicesSubpageBrowserProxy} */
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestOsBluetoothDevicesSubpageBrowserProxy();
    OsBluetoothDevicesSubpageBrowserProxyImpl.setInstanceForTesting(
        browserProxy);
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
    savedDevicesSubpage.remove();
  });

  /**
   * @return {!Promise}
   */
  function init() {
    savedDevicesSubpage =
        document.createElement('os-settings-bluetooth-saved-devices-subpage');
    document.body.appendChild(savedDevicesSubpage);

    flush();
  }

  function getLabel() {
    const label = savedDevicesSubpage.shadowRoot.querySelector('#label');
    if (!label) {
      return '';
    }
    return label.innerText;
  }

  test('Base Test', function() {
    init();
    Router.getInstance().navigateTo(routes.BLUETOOTH_SAVED_DEVICES);
    assertTrue(!!savedDevicesSubpage);
  });

  test('Show list when >0 saved devices', async function() {
    browserProxy.savedDevices = [{name: 'dev1', imageUrl: '', accountKey: '0'}];
    init();

    Router.getInstance().navigateTo(routes.BLUETOOTH_SAVED_DEVICES);
    flushTasks();

    assertEquals(getLabel(), loadTimeData.getString('sublabelWithEmail'));
  });

  test('Show label with no saved devices', async function() {
    init();

    Router.getInstance().navigateTo(routes.BLUETOOTH_SAVED_DEVICES);
    flushTasks();

    assertEquals(getLabel(), loadTimeData.getString('noDevicesWithEmail'));
  });

  test('Show error label', async function() {
    browserProxy.optInStatus =
        FastPairSavedDevicesOptInStatus
            .STATUS_ERROR_RETRIEVING_FROM_FOOTPRINTS_SERVER;
  });

  test('Dynamic dialog change upon last device removal', async function() {
    browserProxy.savedDevices = [{name: 'dev1', imageUrl: '', accountKey: '0'}];
    init();

    Router.getInstance().navigateTo(routes.BLUETOOTH_SAVED_DEVICES);
    flushTasks();

    assertEquals(getLabel(), loadTimeData.getString('sublabelWithEmail'));

    const list =
        savedDevicesSubpage.shadowRoot.querySelector('#savedDevicesList');
    const listItems =
        list.shadowRoot.querySelectorAll('os-settings-saved-devices-list-item');

    assertGT(listItems.length, 0);

    const listItem = listItems[0];

    const ironResizePromise = eventToPromise('iron-resize', list);

    listItem.shadowRoot.querySelector('#dotsMenu').click();
    await flushTasks();
    listItem.shadowRoot.querySelector('#removeButton').click();
    await flushTasks();
    const removeDialog =
        listItem.shadowRoot.querySelector('#removeDeviceDialog');
    removeDialog.shadowRoot.querySelector('#remove').click();

    await ironResizePromise;
    await flushTasks();

    assertEquals(getLabel(), loadTimeData.getString('noDevicesWithEmail'));
  });
});
