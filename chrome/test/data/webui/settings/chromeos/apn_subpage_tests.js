// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('ApnSubpageTest', function() {
  /** @type {ApnSubpageElement} */
  let apnSubpage = null;

  /** @type {?CrosNetworkConfigRemote} */
  let mojoApi_ = null;

  setup(function() {
    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
    PolymerTest.clearBody();
    mojoApi_ = new FakeNetworkConfig();
    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    mojoApi_.setManagedPropertiesForTest(OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular'));
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    apnSubpage = document.createElement('apn-subpage');
    document.body.appendChild(apnSubpage);
    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.APN, params);

    return flushTasks();
  });

  teardown(function() {
    return flushTasks().then(() => {
      apnSubpage.close();
      apnSubpage.remove();
      apnSubpage = null;
      Router.getInstance().resetRouteForTesting();
    });
  });

  test('Check if APN list exists', async function() {
    assertTrue(!!apnSubpage);
    assertTrue(!!apnSubpage.shadowRoot.querySelector('apn-list'));
  });

  test('Page closed while device is updating', async function() {
    const oldClose = apnSubpage.close;
    let counter = 0;
    apnSubpage.close = () => {
      counter++;
      oldClose.apply(apnSubpage, arguments);
    };
    mojoApi_.beforeGetDeviceStateList = () => {
      apnSubpage.close();
    };
    mojoApi_.setDeviceStateForTest({
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      scanning: true,
    });
    await flushTasks();
    assertEquals(1, counter);
    assertFalse(!!apnSubpage.managedProperties_);
  });

  test('Page closed while network is updating', async function() {
    const oldClose = apnSubpage.close;
    let counter = 0;
    apnSubpage.close = () => {
      counter++;
      oldClose.apply(apnSubpage, arguments);
    };
    mojoApi_.beforeGetManagedProperties = () => {
      apnSubpage.close();
    };
    mojoApi_.setManagedPropertiesForTest(OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular'));
    await flushTasks();
    assertEquals(1, counter);
    assertFalse(!!apnSubpage.managedProperties_);
  });

  test(
      'Only updating with different device state should call networkDetails',
      async function() {
        let counter = 0;
        const oldGetNetworkDetails = apnSubpage.getNetworkDetails_;
        apnSubpage.getNetworkDetails_ = () => {
          counter++;
          oldGetNetworkDetails.apply(apnSubpage, arguments);
        };
        mojoApi_.setDeviceStateForTest({
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          macAddress: 'n1',
        });
        await flushTasks();
        // The new device state has the same type as the old, but it is not a
        // full match - we won't call getNetworkDetails.
        assertEquals(0, counter);
        mojoApi_.setDeviceStateForTest({
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          macAddress: 'n1',
        });
        await flushTasks();
        // The new device state has the same type as the old and they fully
        // match - we need to call getNetworkDetails. This is because for
        // cellular networks, some shill device level properties are
        // represented at network level in ONC.
        assertEquals(1, counter);
        mojoApi_.setDeviceStateForTest({
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kDisabled,
          macAddress: 'n2',
        });
        await flushTasks();
        // The new device state is not a full match - we would call
        // getNetworkDetails, because the deviceState has changed.
        assertEquals(2, counter);
        mojoApi_.setDeviceStateForTest({
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          macAddress: 'n2',
        });
        await flushTasks();
        // The new device state is not a full match - we would call
        // getNetworkDetails, because the deviceState has changed.
        assertEquals(3, counter);
        mojoApi_.setDeviceStateForTest({
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          macAddress: 'n2',
          scanning: true,
        });
        await flushTasks();
        // When scanning and type is cellular we always call getNetworkDetails
        assertEquals(4, counter);
      });

  test(
      'Keep same cellular properties while network is updating and' +
          ' scanning is in process',
      async function() {
        let props = OncMojo.getDefaultManagedProperties(
            NetworkType.kCellular, 'cellular_guid', 'cellular');
        props.typeProperties.cellular = {testProp: true};
        mojoApi_.setManagedPropertiesForTest(props);
        await flushTasks();
        apnSubpage.deviceState_ = {
          type: NetworkType.kCellular,
          scanning: true,
        };
        props = OncMojo.getDefaultManagedProperties(
            NetworkType.kCellular, 'cellular_guid', 'cellular');
        props.typeProperties.cellular = {testProp: false};
        mojoApi_.setManagedPropertiesForTest(props);
        await flushTasks();
        assertTrue(
            apnSubpage.managedProperties_.typeProperties.cellular.testProp);
      });

  test('Close is called more than once.', async function() {
    let counter = 0;
    const navigateToPreviousRoute =
        Router.getInstance().navigateToPreviousRoute;
    Router.getInstance().navigateToPreviousRoute = () => {
      counter++;
    };
    apnSubpage.close();
    apnSubpage.close();
    assertEquals(1, counter);
    Router.getInstance().navigateToPreviousRoute = navigateToPreviousRoute;
  });
});
